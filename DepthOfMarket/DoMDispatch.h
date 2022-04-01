/************************************************************************
 * Copyright(c) 2021, One Unified. All rights reserved.                 *
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
 * File:    DoMDispatch.h
 * Author:  raymond@burkholder.net
 * Project: DepthOfMarket
 * Created on October 17, 2021 11:45
 */

#pragma once

#include <map>
#include <string>

#include <TFTimeSeries/DatedDatum.h>

#include <TFIQFeed/Level2/Dispatcher.h>

// NOTE: implement one per symbol
//   might be somewhat faster with out a symbol lookup on each message

// TODO - make a choice
//    pass in dispatch lambdas?
//        then allows multi-purpose lambdas for chart plus trading, etc
//    CRTP with the graph code?  CRTP with multi-purpose code:  chart plus trading?
//       then allows intermediate multi-purpose dispatch via very fast calls

class DoMDispatch
: public ou::tf::iqfeed::l2::Dispatcher<DoMDispatch>
{
  friend ou::tf::iqfeed::l2::Dispatcher<DoMDispatch>;

public:

  DoMDispatch( const std::string& sWatch );
  virtual ~DoMDispatch();

  void EmitMarketMakerMaps();

  void Connect();
  void Disconnect();

protected:

  // called by Network via CRTP
  //void OnNetworkConnected();
  //void OnNetworkDisconnected();
  //void OnNetworkError( size_t e );

  void Initialized();

  void OnMBOAdd( const ou::tf::iqfeed::l2::msg::OrderArrival::decoded& );
  void OnMBOSummary( const ou::tf::iqfeed::l2::msg::OrderArrival::decoded& );
  void OnMBOUpdate( const ou::tf::iqfeed::l2::msg::OrderArrival::decoded& );
  void OnMBODelete( const ou::tf::iqfeed::l2::msg::OrderDelete::decoded& );

private:

  using volume_t = ou::tf::Trade::volume_t;

  std::string m_sWatch;

  struct Order {
    std::string sMarketMaker;
    char chOrderSide;
    double dblPrice;
    volume_t nQuantity;
    uint64_t nPriority;
    uint8_t nPrecision;
    // ptime, if needed
    Order( const ou::tf::iqfeed::l2::msg::OrderArrival::decoded& msg )
    : sMarketMaker( std::move( msg.sMarketMaker ) ), chOrderSide( msg.chOrderSide ),
      dblPrice( msg.dblPrice ), nQuantity( msg.nQuantity ),
      nPriority( msg.nPriority ), nPrecision( msg.nPrecision )
    {}
  };

  struct Auction {
    // maintain set of orders?
    volume_t nQuantity;
    Auction( volume_t nQuantity_ )
    : nQuantity( nQuantity_ ) {}
    Auction( const ou::tf::iqfeed::l2::msg::OrderArrival::decoded& msg )
    : nQuantity( msg.nQuantity ) {}
  };

  using mapOrder_t = std::map<uint64_t,Order>; // key is order id
  mapOrder_t m_mapOrder;

  using mapAuction_t = std::map<double,Auction>;  // key is price
  mapAuction_t m_mapAuctionAsk;
  mapAuction_t m_mapAuctionBid;

  // map to track price levels by market maker
  struct price_mm {
    double price;
    std::string mm;
    price_mm() : price {} {}
    price_mm( double price_, const std::string& mm_ )
    : price( price_ ), mm( mm_ ) {}
    bool operator()( const price_mm& lhs, const price_mm& rhs ) const {
      if ( lhs.price == rhs.price ) {
        return ( lhs.mm < rhs.mm );
      }
      else {
        return ( lhs.price < rhs.price );
      }
    }
  };
  using mapPriceLevels_t = std::map<price_mm, volume_t, price_mm>;
  mapPriceLevels_t m_mapPriceMMAsk;
  mapPriceLevels_t m_mapPriceMMBid;

  struct price_level {
    double price;
    volume_t volume;
    price_level(): price {}, volume {} {}
    price_level( double price_, volume_t volume_ )
    : price( price_ ), volume( volume_ ) {}
  };
  using mapMM_t = std::map<std::string,price_level>;
  mapMM_t m_mapMMAsk;
  mapMM_t m_mapMMBid;

  // OnMBOAdd, OnMBOSummary, OnMBOUpdate (at least for Nasdaq LII )
  void OnMBOOrderArrival( const ou::tf::iqfeed::l2::msg::OrderArrival::decoded& );

  void AuctionAdd( mapAuction_t& map, const ou::tf::iqfeed::l2::msg::OrderArrival::decoded& );
  void AuctionUpdate( mapAuction_t& map, const Order& order, const ou::tf::iqfeed::l2::msg::OrderArrival::decoded& );
  void AuctionDel( mapAuction_t& map, const Order& );

};