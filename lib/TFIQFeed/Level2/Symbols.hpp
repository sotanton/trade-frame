/************************************************************************
 * Copyright(c) 2022, One Unified. All rights reserved.                 *
 * email: info@oneunified.net                                           *
 *                                                                      *
 * This file is provided as is WITHOUT ANY WARRANTY                     *
 *  without even the implied warranty of                                *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                *
 *                                                                      *
 * This software may not be used nor distributed without proper license *
 * agreement.                                                           *
 *                                                                      *
 * See the file LICENSE.txt for redistribution information.             *
 ************************************************************************/

/*
 * File:    Symbols.hpp
 * Author:  raymond@burkholder.net
 * Project: TFIQFeed/Level2
 * Created  April 15, 2022 18:20
 */

#pragma once

#include <memory>

#include <OUCommon/KeyWordMatch.h>

#include <TFTimeSeries/DatedDatum.h>

#include "Dispatcher.h"

namespace ou { // One Unified
namespace tf { // TradeFrame
namespace iqfeed { // IQFeed
namespace l2 { // level 2 data

using volume_t = ou::tf::Trade::volume_t;

// ==== L2Base

class L2Base {  // TODO: convert to CRTP
public:

  using pL2Base_t = std::shared_ptr<L2Base>;
  using fVolumeAtPrice_t = std::function<void(double,int)>;

  L2Base() {}
  virtual ~L2Base() {}

  void Set( fVolumeAtPrice_t&& fBid, fVolumeAtPrice_t&& fAsk ) {
    m_fAskVolumeAtPrice = std::move( fAsk );
    m_fBidVolumeAtPrice = std::move( fBid );
  }

  virtual void OnMBOAdd( const msg::OrderArrival::decoded& ) = 0;
  virtual void OnMBOSummary( const msg::OrderArrival::decoded& ) = 0;
  virtual void OnMBOUpdate( const msg::OrderArrival::decoded& ) = 0;
  virtual void OnMBODelete( const msg::OrderDelete::decoded& ) = 0;

protected:

  fVolumeAtPrice_t m_fBidVolumeAtPrice;
  fVolumeAtPrice_t m_fAskVolumeAtPrice;

  struct Auction {
    // maintain set of orders?
    volume_t nQuantity;
    int nOrders;
    Auction( volume_t nQuantity_ )
    : nQuantity( nQuantity_ ), nOrders( 1 )  {}
    Auction( const ou::tf::iqfeed::l2::msg::OrderArrival::decoded& msg )
    : nQuantity( msg.nQuantity ), nOrders( 1 ) {}
  };

  using mapAuction_t = std::map<double,Auction>;  // key is price
  mapAuction_t m_mapAuctionAsk;
  mapAuction_t m_mapAuctionBid;

  void AuctionAdd(
    const ou::tf::iqfeed::l2::msg::OrderArrival::decoded&,
    fVolumeAtPrice_t&,
    mapAuction_t& );

private:
};

// ==== MarketMaker (Equities, etc)

class MarketMaker: public L2Base {
public:

  using pMarketMaker_t = std::shared_ptr<MarketMaker>;

  MarketMaker(): L2Base() {}
  virtual ~MarketMaker() {}

  static pMarketMaker_t Factory() { return std::make_shared<MarketMaker>(); }

  virtual void OnMBOAdd( const msg::OrderArrival::decoded& ) {}; // Equity doesn't have this message
  virtual void OnMBOSummary( const msg::OrderArrival::decoded& msg ) { OnMBOOrderArrival( msg ); }
  virtual void OnMBOUpdate( const msg::OrderArrival::decoded& msg ) { OnMBOOrderArrival( msg ); }
  virtual void OnMBODelete( const msg::OrderDelete::decoded& );

  void EmitMarketMakerMaps();

protected:
  void OnMBOOrderArrival( const ou::tf::iqfeed::l2::msg::OrderArrival::decoded& msg );
private:

  // updated with OnMBOOrderArrival
  struct price_level {
    double price;
    volume_t volume;
    price_level(): price {}, volume {} {}
    price_level( double price_, volume_t volume_ )
    : price( price_ ), volume( volume_ ) {}
  };
  using mapMM_t = std::map<std::string,price_level>; // key=mm, value=price,volume
  mapMM_t m_mapMMAsk;
  mapMM_t m_mapMMBid;

  void MMAuction_Update(
    const ou::tf::iqfeed::l2::msg::OrderArrival::decoded&,
    fVolumeAtPrice_t&,
    mapMM_t&, mapAuction_t& );
  void MMAuction_Delete(
    const ou::tf::iqfeed::l2::msg::OrderDelete::decoded&,
    fVolumeAtPrice_t&,
    mapMM_t&, mapAuction_t& );
};

// ==== OrderBased (Futures, etc)

class OrderBased: public L2Base {
public:

  using pOrderBased_t = std::shared_ptr<OrderBased>;

  OrderBased(): L2Base() {}
  virtual ~OrderBased() {}

  static pOrderBased_t Factory() { return std::make_shared<OrderBased>(); }

  virtual void OnMBOAdd( const msg::OrderArrival::decoded& msg );
  virtual void OnMBOSummary( const msg::OrderArrival::decoded& msg ) {
    OnMBOAdd( msg );
  } // will this work as expected?
  virtual void OnMBOUpdate( const msg::OrderArrival::decoded& msg );
  virtual void OnMBODelete( const msg::OrderDelete::decoded& msg );

protected:
private:

  struct Order {
    char chOrderSide;
    double dblPrice;
    volume_t nQuantity;
    uint64_t nPriority;
    uint8_t nPrecision;
    // ptime, if needed
    // note: there is no MarketMaker in messages with an order ID
    Order( const ou::tf::iqfeed::l2::msg::OrderArrival::decoded& msg )
    : chOrderSide( msg.chOrderSide ),
      dblPrice( msg.dblPrice ), nQuantity( msg.nQuantity ),
      nPriority( msg.nPriority ), nPrecision( msg.nPrecision )
    { assert( 0 == msg.sMarketMaker.size() ); }
  };

  using mapOrder_t = std::map<uint64_t,Order>; // key is order id
  mapOrder_t m_mapOrder;

  void AuctionUpdate(
    mapAuction_t& map,
    Order& order,
    const ou::tf::iqfeed::l2::msg::OrderArrival::decoded& ,
    fVolumeAtPrice_t&
    );
  void AuctionDel( mapAuction_t& map, const Order&, fVolumeAtPrice_t& );
};

// ==== Carrier for symbol lookup

struct Carrier {

  using pL2Base_t = L2Base*;
  pL2Base_t pL2Base;

  Carrier(): pL2Base {} {}
  Carrier( pL2Base_t p ): pL2Base( p ) {}
  Carrier( const Carrier& rhs ) {
    pL2Base = rhs.pL2Base;
  }
  Carrier& operator=( const Carrier& rhs ) {
    if ( *this != rhs ) {
      pL2Base = rhs.pL2Base;
    }
    return *this;
  }
  Carrier& operator=( pL2Base_t p ) {
    pL2Base = p;
    return *this;
  }
  bool operator!=( const Carrier& rhs ) { return pL2Base != rhs.pL2Base; }
  bool IsNull() { return nullptr == pL2Base; }
};

// ==== Symbols

class Symbols
: public Dispatcher<Symbols>
{
  friend Dispatcher<Symbols>;
public:

  using fConnected_t = std::function<void()>;

  Symbols( fConnected_t&& );
  virtual ~Symbols();

  void Connect();
  void Disconnect();

  using fVolumeAtPrice_t = L2Base::fVolumeAtPrice_t;

  void WatchAdd( const std::string&, fVolumeAtPrice_t&& fBid, fVolumeAtPrice_t&& fAsk );
  void WatchDel( const std::string& );

  void Single( bool );

protected:

  // called by Network via CRTP
  void OnNetworkConnected();
  //void OnNetworkDisconnected();
  //void OnNetworkError( size_t e );
  void OnL2Initialized();

  bool m_bSingle;  // don't use lookups, dedicated to single symbol
  Carrier m_single; // carrier for single symbol

  void OnMBOAdd( const msg::OrderArrival::decoded& );
  void OnMBOSummary( const msg::OrderArrival::decoded& );
  void OnMBOUpdate( const msg::OrderArrival::decoded& );
  void OnMBODelete( const msg::OrderDelete::decoded& );

private:

  fConnected_t m_fConnected;

  ou::KeyWordMatch<Carrier> m_luSymbol;

  struct VolumeAtPriceFunctions {

    fVolumeAtPrice_t fBid;
    fVolumeAtPrice_t fAsk;

    VolumeAtPriceFunctions(
      fVolumeAtPrice_t&& fBid_,
      fVolumeAtPrice_t&& fAsk_
    ): fBid( std::move( fBid_ ) ), fAsk( std::move( fAsk_ ) )
    {}

    VolumeAtPriceFunctions( VolumeAtPriceFunctions&& rhs )
    : fBid( std::move( rhs.fBid ) ), fAsk( std::move( rhs.fAsk ) ) {}
  };

  using mapVolumeAtPriceFunctions_t = std::map<std::string,VolumeAtPriceFunctions>;
  mapVolumeAtPriceFunctions_t m_mapVolumeAtPriceFunctions;

  using pL2Base_t = std::shared_ptr<L2Base>;
  using vL2Base_t = std::vector<pL2Base_t>;
  vL2Base_t m_vL2Base;  // provides end of life destruction

  template<typename Msg>
  void SetCarrier( Carrier& carrier, const Msg& msg ) {

    pL2Base_t pL2Base;
    if ( 0 != msg.nOrderId ) {
      assert( 0 == msg.sMarketMaker.size() );
      pL2Base = OrderBased::Factory();
    }
    else {
      assert( 4 == msg.sMarketMaker.size() );
      pL2Base = MarketMaker::Factory();
    }
    m_vL2Base.push_back( pL2Base );
    carrier = pL2Base.get();

    // may need mutex on this, vs foreground
    mapVolumeAtPriceFunctions_t::iterator iter = m_mapVolumeAtPriceFunctions.find( msg.sSymbolName );
    assert( m_mapVolumeAtPriceFunctions.end() != iter );
    carrier.pL2Base->Set( std::move( iter->second.fBid ), std::move( iter->second.fAsk ) );
    m_mapVolumeAtPriceFunctions.erase( iter );
  }

  template<typename Msg, typename F>
  void Call( const Msg& msg,  F f ) {

    if ( m_bSingle ) {
      if ( m_single.IsNull() ) {
        SetCarrier( m_single, msg );
      }
      (m_single.pL2Base->*f)( msg );
    }
    else {
      Carrier carrier = m_luSymbol.FindMatch( msg.sSymbolName );
      if ( carrier.IsNull() ) {
        SetCarrier( carrier, msg );
        m_luSymbol.AddPattern( msg.sSymbolName, carrier );
      }
      (carrier.pL2Base->*f)( msg );
    }
  }

};

} // namespace l2
} // namesapce iqfeed
} // namespace tf
} // namespace ou
