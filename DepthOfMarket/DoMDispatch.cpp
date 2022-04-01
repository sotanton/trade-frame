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
 * File:    DoMDispatch.cpp
 * Author:  raymond@burkholder.net
 * Project: DepthOfMarket
 * Created on October 17, 2021 11:45
 */

#include "DoMDispatch.h"

using inherited_t = ou::tf::iqfeed::l2::Dispatcher<DoMDispatch>;

DoMDispatch::DoMDispatch( const std::string& sWatch )
: m_sWatch( sWatch )
{
}

DoMDispatch::~DoMDispatch() {
}

void DoMDispatch::EmitMarketMakerMaps() {
  // will probably need a lock on this, as maps are in background thread
  // but mostly works as the deletion isn't in place yet

  m_mapPriceMMAsk.clear();
  m_mapAuctionAsk.clear();

  for ( const mapMM_t::value_type& vt: m_mapMMAsk ) {
    price_mm pmm( vt.second.price, vt.first );
    m_mapPriceMMAsk.emplace( pmm, vt.second.volume );

    mapAuction_t::iterator iter = m_mapAuctionAsk.find( vt.second.price );
    if ( m_mapAuctionAsk.end() == iter ) {
      m_mapAuctionAsk.emplace( vt.second.price, Auction( vt.second.volume ) );
    }
    else {
      iter->second.nQuantity += vt.second.volume;
    }
    //std::cout
    //  << "ask "
    //  << vt.first
    //  << ": " << vt.second.volume
    //  << "@" << vt.second.price
    //  << std::endl;
  }

  m_mapPriceMMBid.clear();
  m_mapAuctionBid.clear();

  for ( const mapMM_t::value_type& vt: m_mapMMBid ) {
    price_mm pmm( vt.second.price, vt.first );
    m_mapPriceMMBid.emplace( pmm, vt.second.volume );

    mapAuction_t::iterator iter = m_mapAuctionBid.find( vt.second.price );
    if ( m_mapAuctionBid.end() == iter ) {
      m_mapAuctionBid.emplace( vt.second.price, Auction( vt.second.volume ) );
    }
    else {
      iter->second.nQuantity += vt.second.volume;
    }
    //std::cout
    //  << "bid "
    //  << vt.first
    //  << ": " << vt.second.volume
    //  << "@" << vt.second.price
    //  << std::endl;
  }

  for (
    mapPriceLevels_t::const_reverse_iterator iter = m_mapPriceMMAsk.rbegin();
    iter != m_mapPriceMMAsk.rend();
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
    mapPriceLevels_t::const_reverse_iterator iter = m_mapPriceMMBid.rbegin();
    iter != m_mapPriceMMBid.rend();
    iter++
  ) {
    std::cout
      << "bid "
      << iter->first.price
      << "," << iter->first.mm
      << "=" << iter->second
      << std::endl;
  }

  for (
    mapAuction_t::const_reverse_iterator iter = m_mapAuctionAsk.rbegin();
    iter != m_mapAuctionAsk.rend();
    iter++
  ) {
    std::cout
      << "ask "
      << iter->first
      << "=" << iter->second.nQuantity
      << std::endl;
  }

  std::cout << "----" << std::endl;

  for (
    mapAuction_t::const_reverse_iterator iter = m_mapAuctionBid.rbegin();
    iter != m_mapAuctionBid.rend();
    iter++
  ) {
    std::cout
      << "bid "
      << iter->first
      << "=" << iter->second.nQuantity
      << std::endl;
  }

}

void DoMDispatch::Connect() {
  inherited_t::Connect();
}

void DoMDispatch::Disconnect() {
  inherited_t::Disconnect();
}

void DoMDispatch::Initialized() {
  StartMarketByOrder( m_sWatch );
  //StartPriceLevel( m_sWatch );
}

void DoMDispatch::OnMBOAdd( const ou::tf::iqfeed::l2::msg::OrderArrival::decoded& msg ) {

  assert( ( '3' == msg.chMsgType ) || ( '6' == msg.chMsgType ) );

  if ( 0 == msg.nOrderId ) { // nasdaq type LII

  }
  else { // other type
    mapOrder_t::iterator iter = m_mapOrder.find( msg.nOrderId );
    if ( m_mapOrder.end() != iter ) {
      std::cout << "map add order already exists: " << msg.nOrderId << std::endl;
    }
    else {
      m_mapOrder.emplace(
        std::pair(
          msg.nOrderId,
          Order( msg ) )
        );
    }

    switch ( msg.chOrderSide ) {
      case 'A':
        AuctionAdd( m_mapAuctionAsk, msg );
        break;
      case 'B':
        AuctionAdd( m_mapAuctionBid, msg );
        break;
    }
  }

}

void DoMDispatch::OnMBOSummary( const ou::tf::iqfeed::l2::msg::OrderArrival::decoded& msg ) {

  assert( '6' == msg.chMsgType );

  if ( 0 == msg.nOrderId ) { // nasdaq LII
    OnMBOOrderArrival( msg );
  }
  else { // regular futures
    OnMBOAdd( msg );  // will this work as expected?
  }

}

void DoMDispatch::OnMBOUpdate( const ou::tf::iqfeed::l2::msg::OrderArrival::decoded& msg ) {

  assert( '4' == msg.chMsgType );

  // for nasdaq l2 on equities, there is on orderid, so will need to lookup by mmid
  // so need a way to distinquish between futures and nasdaq and price levels

  // MBO Futures        WOR WPL L2O|
  // non-MBO Futures    --- WPL ---|
  // Equity Level2      WOR --- ---| has no orderid, use mmid for key?

  // will need to different maps, or skip the lookup in to m_mapOrder

  // equity, nasdaq l2:  '4,SPY,,NSDQ,B,451.4400,300,,4,11:33:27.030724,2022-04-01,'

  if ( 0 == msg.nOrderId ) { // equity/nasdaq lII, map by mm,order side
    OnMBOOrderArrival( msg );
  }
  else { // regular stuff
    mapOrder_t::iterator iter = m_mapOrder.find( msg.nOrderId );
    if ( m_mapOrder.end() == iter ) {
      std::cout << "map update order does not exist: " << msg.nOrderId << std::endl;
    }
    else {

      switch ( msg.chOrderSide ) {
        case 'A':
          AuctionUpdate( m_mapAuctionAsk, iter->second, msg );
          break;
        case 'B':
          AuctionUpdate( m_mapAuctionBid, iter->second, msg );
          break;
      }

      iter->second.nQuantity = msg.nQuantity;
    }
  }


}

// for nasdaq LII
void DoMDispatch::OnMBOOrderArrival( const ou::tf::iqfeed::l2::msg::OrderArrival::decoded& msg ) {

  //if ( "NSDQ" != msg.sMarketMaker ) {
  //  std::cout
  //    << "mmid: "
  //    << msg.sMarketMaker
  //    << "," << msg.chOrderSide
  //    << "," << msg.dblPrice
  //    << "," << msg.nQuantity
  //    << std::endl;
  //}

  price_level pl( msg.dblPrice, msg.nQuantity );
  switch ( msg.chOrderSide ) {
    case 'A':
      {
        mapMM_t::iterator iter = m_mapMMAsk.find( msg.sMarketMaker );
        if ( m_mapMMAsk.end() == iter ) {
          m_mapMMAsk.emplace( msg.sMarketMaker, pl );
        }
        else {
          iter->second = pl;
        }
      }
      break;
    case 'B':
      {
        mapMM_t::iterator iter = m_mapMMBid.find( msg.sMarketMaker );
        if ( m_mapMMBid.end() == iter ) {
          m_mapMMBid.emplace( msg.sMarketMaker, pl );
        }
        else {
          iter->second = pl;
        }
      }
      break;
  }
}

void DoMDispatch::OnMBODelete( const ou::tf::iqfeed::l2::msg::OrderDelete::decoded& msg ) {

  assert( '5' == msg.chMsgType );

  if ( 0 == msg.nOrderId ) { // nasdaq L II
    switch ( msg.chOrderSide ) {
      case 'A':
        {
          mapMM_t::iterator iter = m_mapMMAsk.find( msg.sMarketMaker );
          if ( m_mapMMAsk.end() == iter ) {
            // nothing to do
          }
          else {
            m_mapMMAsk.erase( iter );
          }
        }
        break;
      case 'B':
        {
          mapMM_t::iterator iter = m_mapMMBid.find( msg.sMarketMaker );
          if ( m_mapMMBid.end() == iter ) {
            // nothing to do
          }
          else {
            m_mapMMBid.erase( iter );
          }
        }
        break;
    }
  }
  else { // futures MBO
    mapOrder_t::iterator iter = m_mapOrder.find( msg.nOrderId );
    if ( m_mapOrder.end() == iter ) {
      std::cout << "map order delete does not exist: " << msg.nOrderId << std::endl;
    }
    else {

      switch ( msg.chOrderSide ) {
        case 'A':
          AuctionDel( m_mapAuctionAsk, iter->second );
          break;
        case 'B':
          AuctionDel( m_mapAuctionBid, iter->second );
          break;
      }

      m_mapOrder.erase( iter );
    }
  }


}

void DoMDispatch::AuctionAdd( mapAuction_t& map, const ou::tf::iqfeed::l2::msg::OrderArrival::decoded& msg ) {
  mapAuction_t::iterator iter = map.find( msg.dblPrice );
  if ( map.end() == iter ) {
    map.emplace(
      std::pair(
        msg.dblPrice,
        Auction( msg )
      )
    );
  }
  else {
    iter->second.nQuantity += msg.nQuantity;
  }
}

void DoMDispatch::AuctionUpdate( mapAuction_t& map, const Order& order, const ou::tf::iqfeed::l2::msg::OrderArrival::decoded& msg ) {
  mapAuction_t::iterator iter = map.find( order.dblPrice );
  if ( map.end() == iter ) {
    std::cout << "AuctionUpdate price not found: " << order.sMarketMaker << "," << order.dblPrice << std::endl;
  }
  else {
    auto& nQuantity( iter->second.nQuantity );
    // assert( nQuantity >= order.nQuantity ); // doesn't work for equities, need to fix for mmid
    nQuantity += msg.nQuantity;
    nQuantity -= order.nQuantity;
  }
}

void DoMDispatch::AuctionDel( mapAuction_t& map, const Order& order ) {

  mapAuction_t::iterator iter = map.find( order.dblPrice );
  if ( map.end() == iter ) {
    std::cout << "AuctionDel price not found: " << order.dblPrice << std::endl;
  }
  else {
    iter->second.nQuantity -= order.nQuantity;
  }

}