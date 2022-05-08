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
 * File:    Symbols.cpp
 * Author:  raymond@burkholder.net
 * Project: TFIQFeed/Level2
 * Created  April 15, 2022 18:20
 */

#include <boost/log/trivial.hpp>

#include <OUCommon/TimeSource.h>

#include <TFTrading/KeyTypes.h>

#include "Symbols.hpp"

namespace ou { // One Unified
namespace tf { // TradeFrame
namespace iqfeed { // IQFeed
namespace l2 { // level 2 data

// ==== L2Base

L2Base::L2Base()
: m_fBidVolumeAtPrice( nullptr )
, m_fAskVolumeAtPrice( nullptr )
, m_fMarketDepthByMM( nullptr )
, m_fMarketDepthByOrder( nullptr )
{}

void L2Base::Add( mapLimitOrderAggregate_t& map, fVolumeAtPrice_t& f, price_t price, volume_t volume ) {

  mapLimitOrderAggregate_t::iterator iterLimitOrderAggregate = map.find( price );
  if ( map.end() == iterLimitOrderAggregate ) {
    auto pair = map.emplace( std::pair( price, LimitOrderAggregate( volume ) ) );
    assert( pair.second );
    iterLimitOrderAggregate = pair.first;
  }
  else {
    iterLimitOrderAggregate->second.nQuantity += volume;
    iterLimitOrderAggregate->second.nOrders++;
  }

  if ( f ) f( price, iterLimitOrderAggregate->second.nQuantity, true );
}

void L2Base::Update(
  mapLimitOrderAggregate_t& map, fVolumeAtPrice_t& f,
  price_t old_price, volume_t old_volume,
  price_t new_price, volume_t new_volume
) {
  Delete( map, f, old_price, old_volume );
  Add( map, f, new_price, new_volume );
}

void L2Base::Delete( mapLimitOrderAggregate_t& map, fVolumeAtPrice_t& f, price_t price, volume_t volume ) {

  mapLimitOrderAggregate_t::iterator iterLimitOrderAggregate = map.find( price );
  if ( map.end() == iterLimitOrderAggregate ) {
    BOOST_LOG_TRIVIAL(error) << "L2Base::Delete price not found: " << price;
  }
  else {
    iterLimitOrderAggregate->second.nQuantity -= volume;
    iterLimitOrderAggregate->second.nOrders--;
    // TODO: test that there are zero orders remaining
  }
  if ( f ) f( price, iterLimitOrderAggregate->second.nQuantity, false );

}

// ==== MarketMaker === for nasdaq equities LII

void MarketMaker::OnMBOUpdate( const msg::OrderArrival::decoded& msg ) {

  if ( nullptr != m_fMarketDepthByMM ) {
    ptime dt( ou::TimeSource::Instance().External() );
    ou::tf::DepthByMM md( dt, msg.chMsgType, msg.chOrderSide, msg.nQuantity, msg.dblPrice, msg.mmid.id );
    m_fMarketDepthByMM( md );
  }
  else {
    switch ( msg.chOrderSide ) {
      case 'A':
        MMLimitOrder_Update_Live( msg, m_fAskVolumeAtPrice, m_mapMMAsk, m_mapLimitOrderAggregateAsk );
        break;
      case 'B':
        MMLimitOrder_Update_Live( msg, m_fBidVolumeAtPrice, m_mapMMBid, m_mapLimitOrderAggregateBid );
        break;
    }
  }
}

void MarketMaker::OnMBODelete( const msg::OrderDelete::decoded& msg ) {

  if ( nullptr != m_fMarketDepthByMM ) {
    ptime dt( ou::TimeSource::Instance().External() );
    ou::tf::DepthByMM md( dt, msg.chMsgType, msg.chOrderSide, 0, 0.0, msg.mmid.id );
    m_fMarketDepthByMM( md );
  }
  else {
    switch ( msg.chOrderSide ) {
      case 'A':
        MMLimitOrder_Delete_Live( msg, m_fAskVolumeAtPrice, m_mapMMAsk, m_mapLimitOrderAggregateAsk );
        break;
      case 'B':
        MMLimitOrder_Delete_Live( msg, m_fBidVolumeAtPrice, m_mapMMBid, m_mapLimitOrderAggregateBid );
        break;
    }
  }
}

void MarketMaker::MarketDepth( const ou::tf::DepthByMM& depth ) {
  switch ( depth.MsgType() ) {
    //case 3:  doesn't have add
    case '4': // Update
    case '6': // Summary
      BidOrAsk_Update( depth );
      break;
    case '5':
      BidOrAsk_Delete( depth );
      break;
    default:
      assert( false );
  }
}

void MarketMaker::BidOrAsk_Update( const ou::tf::DepthByMM& depth ) {
  switch ( depth.Side() ) {
    case 'A':
      MMLimitOrder_Update( depth.MMID(), depth.Price(), depth.Volume(),  m_fAskVolumeAtPrice, m_mapMMAsk, m_mapLimitOrderAggregateAsk );
      break;
    case 'B':
      MMLimitOrder_Update( depth.MMID(), depth.Price(), depth.Volume(), m_fBidVolumeAtPrice, m_mapMMBid, m_mapLimitOrderAggregateBid );
      break;
  }
}

void MarketMaker::MMLimitOrder_Update_Live(
  const msg::OrderArrival::decoded& msg,
  fVolumeAtPrice_t& f,
  mapMM_t& mapMM, mapLimitOrderAggregate_t& mapLimitOrderAggregate
) {
  MMLimitOrder_Update( msg.mmid.id, msg.dblPrice, msg.nQuantity, f, mapMM, mapLimitOrderAggregate );
}

void MarketMaker::MMLimitOrder_Update(
  DepthByMM::MMID_t mmid,
  double price, volume_t volume,
  fVolumeAtPrice_t& f,
  mapMM_t& mapMM,
  mapLimitOrderAggregate_t& mapLimitOrderAggregate
) {

  price_level pl( price, volume );

  mapMM_t::iterator mapMM_iter = mapMM.find( mmid );
  if ( mapMM.end() == mapMM_iter ) {
    auto pair = mapMM.emplace( mmid, pl );
    assert( pair.second );
    //mapMM_iter = pair.first;
  }
  else {
    // remove volume from existing price level
    Delete( mapLimitOrderAggregate, f, mapMM_iter->second.price, mapMM_iter->second.volume );
    // assign new price level
    mapMM_iter->second = pl;
  }

  // update new price level
  Add( mapLimitOrderAggregate, f, price, volume );

}

void MarketMaker::MMLimitOrder_Delete_Live(
  const msg::OrderDelete::decoded& msg,
  fVolumeAtPrice_t& f,
  mapMM_t& mapMM,
  mapLimitOrderAggregate_t& mapLimitOrderAggregate
) {
  MMLimitOrder_Delete( msg.mmid.id, f, mapMM, mapLimitOrderAggregate );
}

void MarketMaker::BidOrAsk_Delete( const ou::tf::DepthByMM& depth ) {
  switch ( depth.Side() ) {
    case 'A':
      MMLimitOrder_Delete( depth.MMID(), m_fAskVolumeAtPrice, m_mapMMAsk, m_mapLimitOrderAggregateAsk );
      break;
    case 'B':
      MMLimitOrder_Delete( depth.MMID(), m_fBidVolumeAtPrice, m_mapMMBid, m_mapLimitOrderAggregateBid );
      break;
  }
}

void MarketMaker::MMLimitOrder_Delete(
  DepthByMM::MMID_t mmid, // MMID
  fVolumeAtPrice_t& f,
  mapMM_t& mapMM,
  mapLimitOrderAggregate_t& mapLimitOrderAggregate
) {

  mapMM_t::iterator mapMM_iter = mapMM.find( mmid );
  if ( mapMM.end() == mapMM_iter ) {
    // this should probably be an assert( false )
    // but then will reqquire recovery from stream outages
  }
  else {
    // remove volume from existing price level
    Delete( mapLimitOrderAggregate, f, mapMM_iter->second.price, mapMM_iter->second.volume );
    mapMM.erase( mapMM_iter );
  }

}

void MarketMaker::EmitMarketMakerMaps() {
  // will probably need a lock on this, as maps are in background thread
  // but mostly works as the deletion isn't in place yet

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
  mapPriceLevels_t mapPriceMMAsk;
  mapPriceLevels_t mapPriceMMBid;

  for ( const mapMM_t::value_type& vt: m_mapMMAsk ) {
    price_mm pmm( vt.second.price, ou::tf::DepthByMM::Cast( vt.first ) );
    mapPriceMMAsk.emplace( pmm, vt.second.volume );

    //std::cout
    //  << "ask "
    //  << vt.first
    //  << ": " << vt.second.volume
    //  << "@" << vt.second.price
    //  << std::endl;
  }

  for ( const mapMM_t::value_type& vt: m_mapMMBid ) {
    price_mm pmm( vt.second.price, ou::tf::DepthByMM::Cast( vt.first ) );
    mapPriceMMBid.emplace( pmm, vt.second.volume );

    //std::cout
    //  << "bid "
    //  << vt.first
    //  << ": " << vt.second.volume
    //  << "@" << vt.second.price
    //  << std::endl;
  }

  for (
    mapPriceLevels_t::const_reverse_iterator iter = mapPriceMMAsk.rbegin();
    iter != mapPriceMMAsk.rend();
    iter++
  ) {
    std::cout
      << "ask "
      << iter->first.price
      << "," << iter->first.mm
      << "=" << iter->second
      << std::endl;
  }

  std::cout << "----" << std::endl;

  for (
    mapPriceLevels_t::const_reverse_iterator iter = mapPriceMMBid.rbegin();
    iter != mapPriceMMBid.rend();
    iter++
  ) {
    std::cout
      << "bid "
      << iter->first.price
      << "," << iter->first.mm
      << "=" << iter->second
      << std::endl;
  }

}

// ==== OrderBased

// this may require special processing
void OrderBased::OnMBOSummary( const msg::OrderArrival::decoded& msg ) {
  OnMBOAdd( msg );
}

void OrderBased::OnMBOAdd( const msg::OrderArrival::decoded& msg ) {

  if ( nullptr != m_fMarketDepthByOrder ) {
    ptime dt( ou::TimeSource::Instance().External() );
    ou::tf::DepthByOrder md( dt, msg.dt(), msg.nOrderId, msg.nPriority, msg.chMsgType, msg.chOrderSide, msg.dblPrice, msg.nQuantity );
    m_fMarketDepthByOrder( md );
  }
  else {
    LimitOrderAdd( msg.nOrderId, Order( msg ) );
  }
}

void OrderBased::OnMBOUpdate( const msg::OrderArrival::decoded& msg ) {

  if ( nullptr != m_fMarketDepthByOrder ) {
    ptime dt( ou::TimeSource::Instance().External() );
    ou::tf::DepthByOrder md( dt, msg.dt(), msg.nOrderId, msg.nPriority, msg.chMsgType, msg.chOrderSide, msg.dblPrice, msg.nQuantity );
    m_fMarketDepthByOrder( md );
  }
  else {
    LimitOrderUpdate( msg.nOrderId, msg.chOrderSide, msg.dblPrice, msg.nQuantity );
  }
}

void OrderBased::OnMBODelete( const msg::OrderDelete::decoded& msg ) {

  if ( nullptr != m_fMarketDepthByOrder ) {
    ptime dt( ou::TimeSource::Instance().External() );
    ou::tf::DepthByOrder md( dt, msg.dt(), msg.nOrderId, 0, msg.chMsgType, msg.chOrderSide );
    m_fMarketDepthByOrder( md );
  }
  else {
    LimitOrderDelete( msg.nOrderId );
  }

}

void OrderBased::MarketDepth( const ou::tf::DepthByOrder& depth ) {
  switch ( depth.MsgType() ) {
    case '4': // Update
      LimitOrderUpdate( depth.OrderID(), depth.Side(), depth.Price(), depth.Volume() );
      break;
    case '6': // Summary - will need to categorize this properly
      LimitOrderAdd( depth.OrderID(), Order( depth ) );
      break;
    case '3': // add
      LimitOrderAdd( depth.OrderID(), Order( depth ) );
      break;
    case '5':
      LimitOrderDelete( depth.OrderID() );
      break;
    default:
      assert( false );
  }
}

void OrderBased::LimitOrderAdd( uint64_t nOrderId, const Order& order ) {

  mapOrder_t::iterator iter = m_mapOrder.find( nOrderId );
  if ( m_mapOrder.end() != iter ) {
    //    std::cout << "map add order already exists: " << msg.nOrderId << std::endl;
  }
  else {
    m_mapOrder.emplace( std::pair( nOrderId, order ) );
  }

  switch ( order.chOrderSide ) {
    case 'A':
      Add( m_mapLimitOrderAggregateAsk, m_fAskVolumeAtPrice, order.dblPrice, order.nQuantity );
      break;
    case 'B':
      Add( m_mapLimitOrderAggregateBid, m_fBidVolumeAtPrice, order.dblPrice, order.nQuantity );
      break;
  }

}

void OrderBased::LimitOrderUpdate( uint64_t nOrderId, char chOrderSide, double dblPriceNew, volume_t nQuantityNew ) {

  mapOrder_t::iterator iter = m_mapOrder.find( nOrderId );
  if ( m_mapOrder.end() == iter ) {
    BOOST_LOG_TRIVIAL(error) << "LimitOrderUpdate order does not exist: " << nOrderId;
  }
  else {

    if ( 0 == nQuantityNew ) {
      BOOST_LOG_TRIVIAL(warning) << "LimitOrderUpdate order " << nOrderId << " warning - zero new quantity";
    }

    Order& order( iter->second );
    if ( order.chOrderSide != chOrderSide ) {
      BOOST_LOG_TRIVIAL(error) << "LimitOrderUpdate error - side change " << order.chOrderSide << " to " << chOrderSide;
    }
    else {
      switch ( order.chOrderSide ) {
        case 'A':
          LimitOrderUpdate( order, m_mapLimitOrderAggregateAsk, dblPriceNew, nQuantityNew, m_fAskVolumeAtPrice );
          break;
        case 'B':
          LimitOrderUpdate( order, m_mapLimitOrderAggregateBid, dblPriceNew, nQuantityNew, m_fBidVolumeAtPrice );
          break;
      }
    }
  }
}

void OrderBased::LimitOrderUpdate(
  Order& order,
  mapLimitOrderAggregate_t& map,
  double dblPriceNew,
  volume_t nQuantityNew,
  fVolumeAtPrice_t& f
) {
  // TODO: can this be refactored with caller to minimize the call overhead?
  Delete( map, f, order.dblPrice, order.nQuantity );
  order.dblPrice = dblPriceNew;
  order.nQuantity = nQuantityNew;
  Add( map, f, dblPriceNew, nQuantityNew );

}

void OrderBased::LimitOrderDelete( uint64_t nOrderId ) {

  mapOrder_t::iterator iter = m_mapOrder.find( nOrderId );
  if ( m_mapOrder.end() == iter ) {
    BOOST_LOG_TRIVIAL(error) << "LimitOrderDelete order " << nOrderId << " does not exist";
  }
  else {
    const Order& order( iter->second );
    switch ( order.chOrderSide ) {
      case 'A':
        Delete( m_mapLimitOrderAggregateAsk, m_fAskVolumeAtPrice, order.dblPrice, order.nQuantity );
        break;
      case 'B':
        Delete( m_mapLimitOrderAggregateBid, m_fBidVolumeAtPrice, order.dblPrice, order.nQuantity );
        break;
    }

    m_mapOrder.erase( iter );
  }
}

// ==== Symbols

using inherited_t = Dispatcher<Symbols>;

Symbols::Symbols( fConnected_t&& fConnected )
: inherited_t()
, m_bSingle( false )
, m_fConnected( std::move( fConnected ) )
, m_luSymbol( Carrier(), 20 )
{}

Symbols::~Symbols() {
}

void Symbols::Single( bool bSingle ) {
  if ( bSingle ) {
    assert( 1 >= m_luSymbol.GetNodeCount() );
  }
  else {
    assert( m_single.IsNull() );
  }
  m_bSingle = bSingle;
}

void Symbols::Connect() {
  inherited_t::Connect();
}

void Symbols::Disconnect() {
  inherited_t::Disconnect();
}

void Symbols::OnNetworkConnected() {
}

void Symbols::OnL2Initialized() {
  if ( m_fConnected ) m_fConnected();
}

void Symbols::WatchAdd( const std::string& sSymbol, fVolumeAtPrice_t&& fBid, fVolumeAtPrice_t&& fAsk ) {
  mapL2Base_t::iterator iter = m_mapL2Base.find( sSymbol );
  assert( m_mapL2Base.end() == iter );

  m_mapVolumeAtPriceFunctions.emplace( sSymbol, VolumeAtPriceFunctions( std::move( fBid ), std::move( fAsk) ) );
  StartMarketByOrder( sSymbol );
  // don't add pattern here as Equity/Future is unknown
}

void Symbols::WatchAdd( const std::string& sSymbol, L2Base::fMarketDepthByMM_t&& fMarketDepth ) {
  mapL2Base_t::iterator iter = m_mapL2Base.find( sSymbol );
  assert( m_mapL2Base.end() == iter );

  m_mapMarketDepthFunctionByMM.emplace( sSymbol, std::move( fMarketDepth ) );
  StartMarketByOrder( sSymbol );
}

void Symbols::WatchAdd( const std::string& sSymbol, L2Base::fMarketDepthByOrder_t&& fMarketDepth ) {
  mapL2Base_t::iterator iter = m_mapL2Base.find( sSymbol );
  assert( m_mapL2Base.end() == iter );

  m_mapMarketDepthFunctionByOrder.emplace( sSymbol, std::move( fMarketDepth ) );
  StartMarketByOrder( sSymbol );
}

void Symbols::WatchDel( const std::string& sSymbol ) {
  StopMarketByOrder( sSymbol );
  mapL2Base_t::iterator iter = m_mapL2Base.find( sSymbol );
  //m_mapL2Base.erase( iter );
  // TODO: need to update m_luSymbol/m_single as well
  // TODO: may need some sort of sync if values come in during the meantime
}

  // for nasdaq l2 on equities, there is no orderid, so will need to lookup by mmid
  // so need a way to distinquish between futures and nasdaq and price levels

  // MBO Futures        WOR WPL L2O|
  // non-MBO Futures    --- WPL ---|
  // Equity Level2      WOR --- ---| has no orderid, use mmid for key?

  // will need to different maps, or skip the lookup in to m_mapOrder

  // equity, nasdaq l2:  '4,SPY,,NSDQ,B,451.4400,300,,4,11:33:27.030724,2022-04-01,'

// used with futures, not equities
void Symbols::OnMBOAdd( const msg::OrderArrival::decoded& msg ) {

  assert( ( '3' == msg.chMsgType ) || ( '6' == msg.chMsgType ) );
  Call( msg, &L2Base::OnMBOAdd );
}

void Symbols::OnMBOSummary( const msg::OrderArrival::decoded& msg ) {

  assert( '6' == msg.chMsgType );
  Call( msg, &L2Base::OnMBOSummary );
}

void Symbols::OnMBOUpdate( const msg::OrderArrival::decoded& msg ) {

  assert( '4' == msg.chMsgType );
  Call( msg, &L2Base::OnMBOUpdate );
}

void Symbols::OnMBODelete( const msg::OrderDelete::decoded& msg ) {

  assert( '5' == msg.chMsgType );
  Call( msg, &L2Base::OnMBODelete );
}

} // namespace l2
} // namesapce iqfeed
} // namespace tf
} // namespace ou
