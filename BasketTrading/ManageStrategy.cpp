/************************************************************************
 * Copyright(c) 2010, One Unified. All rights reserved.                 *
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
 * File:   ManageStrategy.cpp
 * Author: raymond@burkholder.net
 *
 * Created on August 26, 2018, 6:46 PM
 */

// 2018/09/04: need to start looking for options which cross strikes on a regular basis both ways
//    count up strikes vs down strikes with each daily bar
//  need a way to enter for combination of shorts and longs to can play market in both directions
//    may be use the 10% logic, or 3 day trend bar logic
//  add live charting for portfolio, position, and instruments
//  run implied atm volatility for underlying

// 2018/09/11
//   change ManagePortfolio:
//     watch opening price/quotes, include that in determination for which symbols to trade
//     may also watch number of option quotes delivered to ensure liquidity
//     then allocate the ToTrade from opening data rather than closing bars
//   output option greeks, and individual portfolio state
//   need a 'resume from over-night' as big moves can happen then, and can make the delta neutral profitable
//   enumerate watching/non-watching entries in the option engine
//   start the AtmIv at open and use to collect some of the above opening trade statistics
//     which also gets the execution contract id in place prior to the trade
//   delta should be >0.40 to enter
//   use option with >7 days to expiry?
//   add charts to watch AtmIv, options, underlying (steal from ComboTrading)
//   add a roll-down maneouver to keep delta close to 1
//   need option exit, and install a stop when in the profit zone, and put not longer necessary
//   need to get out of an option at $0.20, otherwise may not be a market
//     add a stop?  (do not worry when doing multi-day trades)
//
// 2019/05/03 adjust trading for first hour and last hour of trading day?

    /* 2019/05/06
     * For orders, opening, as well as closing
     * find atm strike (done)
     * create option at strike (done)
     * verify that quotes are within designated spread (done)
     * create position (done)
     * create order from position, submit as limit at midpoint (done)
     * periodically, if order still executing,
     *   update middiff, +/- based upon buy or sell (done)
     * => need call,put spreads to be < 0.10 && >= 0.01 (for a 6s interval)  (done)
     * => adjacent strikes need to be within 0.51 (done)
     * => a roll up or down needs to retain some profit after commission and spread
     * => roll once directional momentum on underlying has changed
     * => check open interest
     * => needs to be multi-day affair to reduce entry/exit spreads/commissions
     * => need end of week calendar roll, preferably when already about to roll on a strike
     *       start wed/thurs on the calendar rolls
     * => autonomously monitor entries, seek confirmation from money manager prior to entry
     * => allow daily and long term portfolios (allows another attempt at the ema strategy)
     * => to reduce symbol count, load up call first to examine spread, then load up put for verification?
     */

// 2019/06/11 add zig-zag - based on historical 2sd range / 10?
// 2019/06/11 use 2sd historical range for determining edges of spreads and 1 by 2 back spreads

// 2019/05/23 Trading Day
//   ES dropped from 2056 at futures open to about 2016 in the morning (-1.15->-1.2% drop)
//   strangles were profitable on the basket elements
//   profitable legs could be exited
//   TODO: watch ATM IV to see if profitable legs should be rolled-up/down or just exited
//      and new strikes entered when IV returns closer to noral
//      or sell premium(short the same leg?)

#include <algorithm>

#include <TFOptionCombos/Collar.h>
using combo_t = ou::tf::option::Collar;
#include <TFOptionCombos/LegNote.h>

#include <TFVuTrading/TreeItem.hpp>

#include "ManageStrategy.h"
#include "OptionRegistry.hpp"

namespace {
  ou::Colour::EColour rColour[] = {
    ou::Colour::DarkBlue,
    ou::Colour::DarkCyan,
    ou::Colour::MediumSlateBlue,
    ou::Colour::SteelBlue,
    ou::Colour::DarkOrange,
    ou::Colour::MediumTurquoise,
    ou::Colour::DarkOrchid,
    ou::Colour::DarkMagenta,
    ou::Colour::DeepPink,
    ou::Colour::MediumPurple,
    ou::Colour::MediumBlue
  };
}

// == ManageStrategy

ManageStrategy::ManageStrategy(
  //const ou::tf::Bar& barPriorDaily,
  double dblPivot
, pWatch_t pWatchUnderlying
, pPortfolio_t pPortfolioOwning // => owning portfolio
, const ou::tf::option::SpreadSpecs& specSpread
, fGatherOptions_t&& fGatherOptions
  //fConstructWatch_t fConstructWatch, // => m_fConstructWatch underlying
, fConstructOption_t&& fConstructOption // => m_fConstructOption
, fConstructPosition_t&& fConstructPosition // => m_fConstructPosition
, fConstructPortfolio_t&& fConstructPortfolio // => m_fConstructPortfolio
, fRegisterOption_t&& fRegisterOption // => m_fRegisterOption
, fStartCalc_t&& fStartCalc // => m_fStartCalc
, fStopCalc_t&& fStopCalc // => m_fStopCalc
, fSetChartDataView_t&& fSetChartDataView // => m_fSetChartDataView
, fFirstTrade_t fFirstTrade // => m_fFirstTrade
, fAuthorizeUnderlying_t fAuthorizeUnderlying // => m_fAuthorizeUnderlying
, fAuthorizeOption_t fAuthorizeOption // => m_fAuthorizeOption
, fAuthorizeSimple_t fAuthorizeSimple // => m_fAuthorizeSimple
, fBar_t fBar
  )
: ou::tf::DailyTradeTimeFrame<ManageStrategy>(),
  m_dblOpen {},
  m_dblPivot( dblPivot ),
  //m_barPriorDaily( barPriorDaily ),
  m_pWatchUnderlying( pWatchUnderlying ),
  m_pPortfolioOwning( pPortfolioOwning ),

  m_specsSpread( specSpread ),

  m_ptiSelf( nullptr ),

  m_fConstructOption( std::move( fConstructOption ) ),
  m_fConstructPosition( std::move( fConstructPosition ) ),
  m_fConstructPortfolio( std::move( fConstructPortfolio ) ),
  m_stateTrading( ETradingState::TSInitializing ),
  m_fFirstTrade( fFirstTrade ),
  m_fAuthorizeUnderlying( fAuthorizeUnderlying ),
  m_fAuthorizeOption( fAuthorizeOption ),
  m_fAuthorizeSimple( fAuthorizeSimple ),
  m_fBar( fBar ),

  m_dblStrikeCurrent {},
  m_dblPriceCurrent {},

  m_fSetChartDataView( std::move( fSetChartDataView ) ),

  m_eTradeDirection( ETradeDirection::None ),
  m_bfQuotes01Sec( 1 ),
  m_bfTrades01Sec( 1 ),
  m_bfTrades06Sec( 6 ),
//  m_bfTicks06sec( 6 ),
//  m_bfTrades60Sec( 60 ),
//  m_cntUpReturn {}, m_cntDnReturn {},

  m_stateEma( EmaState::EmaUnstable ),
  //m_eOptionState( EOptionState::Initial1 ),

  m_ixColour {},
  m_ceShortEntries( ou::ChartEntryShape::EShape::Short, ou::Colour::Red ),
  m_ceLongEntries( ou::ChartEntryShape::EShape::Long, ou::Colour::Blue ),
  m_ceShortFills( ou::ChartEntryShape::EShape::FillShort, ou::Colour::Red ),
  m_ceLongFills( ou::ChartEntryShape::EShape::FillLong, ou::Colour::Blue ),
  m_ceShortExits( ou::ChartEntryShape::EShape::ShortStop, ou::Colour::Red ),
  m_ceLongExits( ou::ChartEntryShape::EShape::LongStop, ou::Colour::Blue )
{
  assert( m_pWatchUnderlying );
  assert( m_pPortfolioOwning );

  assert( fGatherOptions );
  //assert( nullptr != m_fConstructWatch );
  assert( m_fConstructOption );
  assert( m_fConstructPosition );
  assert( m_fConstructPortfolio );
  assert( m_fFirstTrade );
  assert( m_fBar );

  //m_rBarDirection[ 0 ] = EBarDirection::None;
  //m_rBarDirection[ 1 ] = EBarDirection::None;
  //m_rBarDirection[ 2 ] = EBarDirection::None;

  m_pChartDataView = std::make_shared<ou::ChartDataView>();

  m_pChartDataView->SetNames( "Unknown Strategy", m_pWatchUnderlying->GetInstrument()->GetInstrumentName() );

  //m_ceUpReturn.SetName( "Up Return" );
  //m_ceDnReturn.SetName( "Dn Return" );
  m_ceProfitLossStrategy.SetName( "P/L Strategy" );

  //m_ceUpReturn.SetColour( ou::Colour::Red );
  //m_ceDnReturn.SetColour( ou::Colour::Blue );
  m_ceProfitLossStrategy.SetColour( ou::Colour::Fuchsia );

  m_pChartDataView->Add( EChartSlot::PL, &m_ceProfitLossStrategy );

  //pChartDataView->Add( EChartSlot::Tick, &m_ceTickCount );

  //pChartDataView->Add( 4, &m_ceUpReturn );
  //pChartDataView->Add( 4, &m_ceDnReturn );

  m_pChartDataView->Add( EChartSlot::Price, &m_ceShortEntries );
  m_pChartDataView->Add( EChartSlot::Price, &m_ceLongEntries );
  m_pChartDataView->Add( EChartSlot::Price, &m_ceShortFills );
  m_pChartDataView->Add( EChartSlot::Price, &m_ceLongFills );
  m_pChartDataView->Add( EChartSlot::Price, &m_ceShortExits );
  m_pChartDataView->Add( EChartSlot::Price, &m_ceLongExits );

  m_pOptionRegistry = std::make_unique<OptionRegistry>(
    std::move( fRegisterOption ),
    std::move( fStartCalc ),
    std::move( fStopCalc ),
    [this]( pChartDataView_t p ){
      m_fSetChartDataView( p );
    }
  );

  m_bfQuotes01Sec.SetOnBarComplete( MakeDelegate( this, &ManageStrategy::HandleBarQuotes01Sec ) );
  m_bfTrades01Sec.SetOnBarComplete( MakeDelegate( this, &ManageStrategy::HandleBarTrades01Sec ) );
  m_bfTrades06Sec.SetOnBarComplete( MakeDelegate( this, &ManageStrategy::HandleBarTrades06Sec ) );
  //m_bfTicks06sec.SetOnBarComplete( MakeDelegate( this, &ManageStrategy::HandleBarTicks06Sec ) );
  //m_bfTrades60Sec.SetOnBarComplete( MakeDelegate( this, &ManageStrategy::HandleBarTrades60Sec ) );

  pInstrument_t pInstrumentUnderlying = m_pWatchUnderlying->GetInstrument();

  try {

    // perform this validation for the options instead (but only when IB is the execution provider)
    //if ( 0 == pInstrumentUnderlying->GetContract() ) {
    //  std::cout << pInstrumentUnderlying->GetInstrumentName() << " has no contract" << std::endl;
    //  m_stateTrading = TSNoMore;
    //}

    m_pOptionRegistry->AssignWatchUnderlying( m_pWatchUnderlying );

    // collect option chains for the underlying
    // TODO: this will be passed in
    auto fGatherOptions_ = fGatherOptions;
    ou::tf::option::PopulateMap<mapChains_t>(
      m_mapChains,
      pWatchUnderlying->GetInstrument()->GetInstrumentName(),
      std::move( fGatherOptions_ )
      );

    assert( 0 != m_mapChains.size() );

    std::vector<boost::gregorian::date> vChainsToBeRemoved;

    size_t nStrikesSum {};
    for ( const mapChains_t::value_type& vt: m_mapChains ) { // only use chains where all calls/puts available

      size_t nStrikesTotal {};
      size_t nStrikesMatch {};

      std::vector<double> vMisMatch;
      vMisMatch.reserve( 10 );

      vt.second.Strikes(
        [&nStrikesTotal,&nStrikesMatch,&vMisMatch]( double strike, const chain_t::strike_t& options ){
          nStrikesTotal++;
          if ( options.call.sIQFeedSymbolName.empty() || options.put.sIQFeedSymbolName.empty() ) {
            vMisMatch.push_back( strike );
          }
          else {
            nStrikesMatch++;
          }
      } );

      if ( nStrikesTotal == nStrikesMatch ) {
        nStrikesSum += nStrikesTotal;
        std::cout << "chain " << vt.first << " added with " << nStrikesTotal << " strikes" << std::endl;
      }
      else {
        if ( 0 == nStrikesMatch ) {
          std::cout << "chain " << vt.first << " skipped with " << nStrikesMatch << '/' << nStrikesTotal << " strikes" << std::endl;
          vChainsToBeRemoved.push_back( vt.first );
        }
        else {
          std::cout
            << "chain " << vt.first << " added " << nStrikesMatch << " strikes without";
          for ( double strike: vMisMatch ) {
            std::cout << " " << strike;
            const_cast<chain_t&>( vt.second ).Erase( strike );
          }
          std::cout << std::endl;
          assert( 0 < vt.second.Size() );
        }
      }
    }

    assert( vChainsToBeRemoved.size() != m_mapChains.size() );
    for ( auto date: vChainsToBeRemoved ) { // remove chains with incomplete info
      mapChains_t::iterator iter = m_mapChains.find( date );
      m_mapChains.erase( iter );
    }

    size_t nAverageStrikes = nStrikesSum / m_mapChains.size();
    std::cout << "chain size average: " << nAverageStrikes << std::endl;

    m_pWatchUnderlying->OnQuote.Add( MakeDelegate( this, &ManageStrategy::HandleQuoteUnderlying ) );
    m_pWatchUnderlying->OnTrade.Add( MakeDelegate( this, &ManageStrategy::HandleTradeUnderlying ) );

    m_pValidateOptions = std::make_unique<ValidateOptions>(
        m_pWatchUnderlying,
        m_mapChains,
        m_fConstructOption
      );
    m_pValidateOptions->SetSize( combo_t::LegCount() ); // will need to make this generic

  }
  catch (...) {
    std::cout << "*** " << "something wrong with " << pInstrumentUnderlying->GetInstrumentName() << " creation." << std::endl;
  }

}

ManageStrategy::~ManageStrategy( ) {

  m_stateTrading = ETradingState::TSNoMore;

  ManageIVTracker_End();

  m_pWatchUnderlying->OnQuote.Remove( MakeDelegate( this, &ManageStrategy::HandleQuoteUnderlying ) );
  m_pWatchUnderlying->OnTrade.Remove( MakeDelegate( this, &ManageStrategy::HandleTradeUnderlying ) );

  m_pCombo.reset();
  m_pOptionRegistry.reset();
  m_vEMA.clear();
}

void ManageStrategy::SetTreeItem( ou::tf::TreeItem* ptiSelf ) {
  m_ptiSelf = ptiSelf;
  m_pOptionRegistry->SetTreeItem( ptiSelf );
  }

void ManageStrategy::Run() {
  assert( m_pWatchUnderlying );
  if ( ETradingState::TSInitializing == m_stateTrading ) {
    m_stateTrading = TSWaitForFirstTrade;
  }
}

// is this used currently?
ou::tf::DatedDatum::volume_t ManageStrategy::CalcShareCount( double dblFunds ) const {
  volume_t nOptionContractsToTrade {};
  const std::string& sUnderlying( m_pWatchUnderlying->GetInstrument()->GetInstrumentName() );
  if ( 0.0 != m_dblOpen ) {
    nOptionContractsToTrade = ( (volume_t)std::floor( dblFunds / m_dblOpen ) )/ 100;
    std::cout << sUnderlying << " funds on open: " << dblFunds << ", " << m_dblOpen << ", " << nOptionContractsToTrade << std::endl;
  }
  else {
    //nOptionContractsToTrade = ( (volume_t)std::floor( dblFunds / m_barPriorDaily.Close() ) )/ 100;
    nOptionContractsToTrade = 1;
    //std::cout << sUnderlying << " funds on bar close: " << dblFunds << ", " << m_barPriorDaily.Close() << ", " << nOptionContractsToTrade << std::endl;
  }

  volume_t nUnderlyingSharesToTrade = nOptionContractsToTrade * 100;  // round down to nearest 100
  std::cout << sUnderlying << " funds: " << nOptionContractsToTrade << ", " << nUnderlyingSharesToTrade << std::endl;
  return nUnderlyingSharesToTrade;
}

// add pre-existing positions from database
// NOTE: are there out of order problems, as Collar vLeg is ordered in a particular manner?  LegInfo may have now resolved this
void ManageStrategy::AddPosition( pPosition_t pPosition ) {

  pInstrument_t pInstrument = pPosition->GetInstrument();
  pWatch_t pWatch = pPosition->GetWatch();
  switch ( pInstrument->GetInstrumentType() ) {
    case ou::tf::InstrumentType::Stock:
    case ou::tf::InstrumentType::Future:
      //assert( m_pPositionUnderlying );
      //assert( pPosition->GetInstrument()->GetInstrumentName() == m_pPositionUnderlying->GetInstrument()->GetInstrumentName() );
      //assert( pPosition.get() == m_pPositionUnderlying.get() );
      std::cout << "ManageStrategy::AddPosition adding underlying position, needs additional code: " << pInstrument->GetInstrumentName() << std::endl;
      break;
    case ou::tf::InstrumentType::Option:
    case ou::tf::InstrumentType::FuturesOption:
      if ( pPosition->IsActive() ) {

        const idPortfolio_t idPortfolio = pPosition->GetRow().idPortfolio;

        combo_t* pCombo;
        if ( m_pCombo ) {
          // use existing combo
          pCombo = &dynamic_cast<combo_t&>( *m_pCombo );
          assert( pCombo );
        }
        else {
           // need to construct empty combo when first leg presented

          m_pCombo = std::make_unique<combo_t>();

          ComboPrepare( GetNoon().date() );

          pCombo = &dynamic_cast<combo_t&>( *m_pCombo );
          assert( pCombo );

          // NOTE: as portfolios are created only on validation, are all portfolios on the tree, but marked as authorized or not?
          //    < not quite true, portolios may be loaded from previous session and need to be marked as pre-existing >
          //   ie, maybe status markers:  loaded, pre-existing, authorized, not-authorized

          pInstrument_t pInstrumentUnderlying = m_pWatchUnderlying->GetInstrument();
          const std::string& sNameUnderlying( pInstrumentUnderlying->GetInstrumentName() );

          pCombo->SetPortfolio( m_fConstructPortfolio( idPortfolio, m_pPortfolioOwning->Id() ) );
          m_ptiSelf->UpdateText( idPortfolio );
          m_pChartDataView->SetNames( idPortfolio, sNameUnderlying );

          // authorizes pre-existing strategy
          //bool bAuthorized = m_fAuthorizeSimple( idPortfolio, sNameUnderlying, true );

        } // ensure m_pCombo is available

        std::cout << "set combo option position existing: " << pWatch->GetInstrument()->GetInstrumentName() << std::endl;

        // TODO: may need special call for colour for non-Open positions
        using LegNote = ou::tf::option::LegNote;
        //const LegNote::values_t& lnValues = pCombo->SetPosition( pPosition, m_pChartDataView, rColour[ m_ixColour++ ] );
        const LegNote::values_t& lnValues = pCombo->SetPosition( pPosition );
        if ( LegNote::State::Open == lnValues.m_state ) {
        }
        else {
          m_ixColour--;  // TODO: look at this again, is this proper?
        }
      }
      break;
    default:
      assert( false );
  }
}

void ManageStrategy::ClosePositions( void ) {
  std::cout << m_pWatchUnderlying->GetInstrument()->GetInstrumentName() << " close positions" << std::endl;
  if ( m_pCombo ) {
    combo_t& combo( dynamic_cast<combo_t&>( *m_pCombo ) );
    combo.CancelOrders(); // TODO: generify via Common or Base
    combo.ClosePositions(); // TODO: generify via Common or Base
  }
}

void ManageStrategy::HandleBellHeard( boost::gregorian::date, boost::posix_time::time_duration ) {
}

void ManageStrategy::HandleQuoteUnderlying( const ou::tf::Quote& quote ) {
//  if ( quote.IsValid() ) {  // far out of the money options have a 0.0 bid
    m_QuoteUnderlyingLatest = quote;
    m_bfQuotes01Sec.Add( quote.DateTime(), quote.Spread(), 1 );
//    m_quotes.Append( quote );
    TimeTick( quote );
//  }
}

void ManageStrategy::HandleTradeUnderlying( const ou::tf::Trade& trade ) {
//  if ( trade.Price() > m_TradeLatest.Price() ) m_cntUpReturn++;
//  if ( trade.Price() < m_TradeLatest.Price() ) m_cntDnReturn--;
//  m_trades.Append( trade );
  //m_bfTicks06sec.Add( trade.DateTime(), 0, 1 );

  m_dblPriceCurrent = trade.Price();

  m_bfTrades01Sec.Add( trade );
  m_bfTrades06Sec.Add( trade );
//  m_bfTrades60Sec.Add( trade );
  TimeTick( trade );
  m_TradeUnderlyingLatest = trade; // allow previous one to be used till last moment
}

void ManageStrategy::HandleRHTrading( const ou::tf::Quote& quote ) {
  switch ( m_stateTrading ) {
    case TSWaitForEntry:
      break;
    case TSMonitorLong:
      // TODO: monitor for delta changes, or being able to roll down to next strike
      // TODO: for over night, roll on one or two days.
      //    on up trend: drop puts once in profit zone, and set trailing stop
      //    on down trend:  keep rolling down each strike or selling on delta change of 1, and revert to up-trend logic
      break;
    case TSMonitorShort:
      break;
  }
}

void ManageStrategy::HandleRHTrading( const ou::tf::Trade& trade ) {
  switch ( m_stateTrading ) {
    case TSWaitForFirstTrade: {
      m_dblOpen = trade.Price();
      //std::cout << m_pWatchUnderlying->GetInstrumentName() << " " << trade.DateTime() << ": First Price: " << trade.Price() << std::endl;
      m_fFirstTrade( *this, trade );
      m_stateTrading = TSOptionEvaluation; // ready to trade
      }
      break;
//    case TSWaitForFundsAllocation: // Start() needs to be called
//      break;
    case TSWaitForEntry:
      break;
    case TSOptionEvaluation:
      break;
//    case TSMonitorCombo:
//      break;
    case TSMonitorLong: {
      pEMA_t& pEMA( m_vEMA.back() );
      if ( trade.Price() < pEMA->dblEmaLatest ) {
        //m_pPositionUnderlying->ClosePosition( ou::tf::OrderType::Market );
        //m_ceLongExits.AddLabel( trade.DateTime(), trade.Price(), "Stop" );
        //std::cout << m_sUnderlying << " closing long" << std::endl;
        m_stateTrading = TSWaitForEntry;
      }
      }
      break;
    case TSMonitorShort: {
      pEMA_t& pEMA( m_vEMA.back() );
      if ( trade.Price() > pEMA->dblEmaLatest ) {
        //m_pPositionUnderlying->ClosePosition( ou::tf::OrderType::Market );
        //m_ceShortExits.AddLabel( trade.DateTime(), trade.Price(), "Stop" );
        //std::cout << m_sUnderlying << " closing short" << std::endl;
        m_stateTrading = TSWaitForEntry;
      }
      }
      break;
    default:
      break;
  }
}

void ManageStrategy::HandleRHTrading( const ou::tf::Bar& bar ) { // one second bars, currently composite of quote spreads
  // this is one tick behind, so could use m_TradeLatest for latest close-of-last/open-of-next
  //RHEquity( bar );

  //const double mid = m_QuoteUnderlyingLatest.Midpoint();
  // BollingerTransitions::Crossing( mid ) // TODO: needs to be migrated to Underlying

  //ManageIVTracker_RH();

  RHOption( bar );
}

void ManageStrategy::ComboPrepare( boost::gregorian::date date ) {

  const std::string& sUnderlying( m_pWatchUnderlying->GetInstrumentName() );
  std::cout << "ManageStrategy::ComboPrepare: " << sUnderlying << std::endl;

  m_pCombo->Prepare(
    date, &m_mapChains, m_specsSpread,
    [this]( const std::string& sOptionName, combo_t::fConstructedOption_t&& fConstructedOption ){ // fConstructOption_t
      // TODO: maintain a local map for quick reference
      m_fConstructOption(
        sOptionName,
        [ f=std::move( fConstructedOption ) ]( pOption_t pOption ){
          f( pOption );
        }
      );
    },
    [this]( pOption_t pOption, pPosition_t pPosition, const std::string& sLegType, ou::tf::option::Combo::vMenuActivation_t&& ma ) { // fActivateOption_t
      //std::cout << "Option repository: adding option " << pOption->GetInstrumentName() << std::endl;
      m_pOptionRegistry->Add( pOption, pPosition, sLegType, std::move( ma ) );
    },
    [this]( ou::tf::option::Combo* p, pOption_t pOption, const std::string& note )->pPosition_t { // fConstructPosition_t
      combo_t* pCombo = reinterpret_cast<combo_t*>( p );
      pPosition_t pPosition = m_fConstructPosition( pCombo->GetPortfolio()->GetRow().idPortfolio, pOption, note );
      using LegNote = ou::tf::option::LegNote;
      const LegNote::values_t& lnValues = pCombo->SetPosition( pPosition );
      //p->PlaceOrder( lnValues.m_type, ou::tf::OrderSide::Buy, 1 );  // TODO: perform this in the combo, rename to AddPosition?
      return pPosition;
    },
    [this]( pOption_t pOption ){ // fDeactivateOption_t
      //std::cout << "Option repository: removing option " << pOption->GetInstrumentName() << std::endl;
      //assert( pWatch->GetInstrument()->IsOption() );
      //pOption_t pOption = std::dynamic_pointer_cast<ou::tf::option::Option>( pWatch );
      m_pOptionRegistry->Remove( pOption, true );
    }
    );

  m_pCombo->SetChartData( m_pChartDataView, rColour[ m_ixColour++ ] );

}

/*
 * TODO:
 *   split out each option strategy
 *   add additional states
* rules:
 *   expiry day:
 *     atm, itm : roll, same strike
 *     otm: expire
 *          re-enter at atm?
 *          zero-price, zero-cost close/expire for the position
 */

void ManageStrategy::RHOption( const ou::tf::Bar& bar ) { // assumes one second bars, currently a bar of quote spreads

  // what happens with Add()?  What state are we in?  What states are executed to reach here?
  // need to determine states and then sequence to get the combo initialized

  const double mid = m_QuoteUnderlyingLatest.Midpoint();

  switch ( m_stateTrading ) {
    case TSOptionEvaluation:
      {

        if ( !m_pCombo ) {

          const std::string& sUnderlying( m_pWatchUnderlying->GetInstrument()->GetInstrumentName() );

          try {
            const ou::tf::option::Combo::E20DayDirection direction
              = ( m_dblPivot <= mid )
              ? ou::tf::option::Combo::E20DayDirection::Rising
              : ou::tf::option::Combo::E20DayDirection::Falling;
            const boost::gregorian::date dateBar( bar.DateTime().date() );
            if ( m_pValidateOptions->ValidateBidAsk(
              dateBar, mid, 11,
              [this,mid,direction]( const mapChains_t& chains, boost::gregorian::date date, double price, combo_t::fLegSelected_t&& fLegSelected ){
                combo_t::ChooseLegs( direction, chains, date, m_specsSpread, mid, fLegSelected );
              }
            ) ) {

              const idPortfolio_t idPortfolio
                = combo_t::Name( direction, m_mapChains, dateBar, m_specsSpread, mid ,sUnderlying );

              if ( m_fAuthorizeSimple( idPortfolio, sUnderlying, false ) ) {

                std::cout << sUnderlying << ": bid/ask spread ok, opening positions (pivot=" << m_dblPivot << "/" << mid << ")" << std::endl;

                m_pCombo = std::make_unique<combo_t>();
                assert( m_pCombo );

                ComboPrepare( GetNoon().date() );

                combo_t& combo = dynamic_cast<combo_t&>( *m_pCombo );

                if ( m_ixColour >= ( sizeof( rColour ) - 2 ) ) {
                  std::cout << "WARNING: strategy running out of colours." << std::endl;
                }
                std::cout << sUnderlying << " construct portfolio: " << m_pPortfolioOwning->Id() << " adds " << idPortfolio << std::endl;
                combo.SetPortfolio( m_fConstructPortfolio( idPortfolio, m_pPortfolioOwning->Id() ) );
                m_ptiSelf->UpdateText( idPortfolio );
                m_pChartDataView->SetNames( idPortfolio, m_pWatchUnderlying->GetInstrument()->GetInstrumentName() );

                m_pValidateOptions->Get( // process each option of set
                  [this,&idPortfolio,&combo,direction]( size_t ix, pOption_t pOption ){ // fValidatedOption_t -- need Strategy specific naming
                    // called for each of the legs
                    ou::tf::option::LegNote::values_t lnValues;
                    combo_t::FillLegNote( ix, direction, lnValues );
                    ou::tf::option::LegNote ln( lnValues );
                    pPosition_t pPosition = m_fConstructPosition( idPortfolio, pOption, ln.Encode() );
                    assert( pPosition );
                    combo.SetPosition( pPosition );
                    }
                  );

                m_pValidateOptions->ClearValidation(); // after positions created to keep watch in options from a quick stop/start

                combo.PlaceOrder( ou::tf::OrderSide::Buy, 1 );
                m_stateTrading = ETradingState::TSComboMonitor;

              } // m_fAuthorizeSimple
              else {
                // ?
              }
              // TODO: re-use existing combo?  what if leg is still active? add one or both legs?  if not profitable, no use adding to loss leg
              // TODO: create a trailing stop based upon entry net loss?
            }
          }
          catch ( const ou::tf::option::exception_strike_range_exceeded& e ) {
            // don't worry about this, price is not with in range yet
          }
          catch ( const std::runtime_error& e ) {
            std::cout << sUnderlying << " run time error, stop trading: " << e.what() << std::endl;
            m_pValidateOptions->ClearValidation();
            m_stateTrading = TSNoMore;  // TODO: fix this for multiple combos in place
          }
        } // m_bAllowedComboAdd
        else {
          m_stateTrading = ETradingState::TSComboMonitor;  // state machine needs to be confirmed
        }

      }
      break;
//    case TSWaitForEntry:
//      break;
    case TSComboMonitor:
      {
        // TODO: track pivot crossing, track momentun
        //   pivot acts as stop, momentum can carry through
        //     if momentum changes first, then roll
        //     if momentum doesn't change, but trades back over pivot, then roll
        //     don't roll if not profitable yet (commision plus bid/ask spread )

        //PivotCrossing::ECrossing crossing = m_pivotCrossing.Update( mid );
        // TODO: need to cross upwards for calls, cross downwards for puts (for long strangle)
//        if ( PivotCrossing::ECrossing::none != crossing ) {
//          for ( mapCombo_t::value_type& vt: m_mapCombo ) {
            //bClosed |= vt.second.CloseItmLegForProfit( mid );
//            namespace ph = std::placeholders;
//            vt.second.CloseItmLegForProfit(
//              mid,
//              m_DefaultOrderSide, // for new entry
//              std::bind( &ManageStrategy::BuildPosition, this, ph::_1, ph::_2, ph::_3, std::move( ph::_4 ) )
//              );
            // implement trailing stop or parabolic SAR
            // how wide to set the stop?  double the average jitter in price?
            // maybe the roll should be to sell the next otm.  depends on how fast moving
            // use the crossing for the trigger for the trailing stop
//          }
//        }

        if ( 4 == m_vEMA.size() ) { // on second day, is m_vEMA built?
          double slope( m_vEMA[2]->dblEmaLatest - m_vEMA[3]->dblEmaLatest ); // fast - slow
          m_pCombo->Tick( slope, mid, bar.DateTime() ); // TODO: need to pass slope of underlying
        }

      }
      break;
    default:
      break;
  }
}

void ManageStrategy::ManageIVTracker_BuildRow() {

  auto fAddOption =
    [this]( pOption_t pOption ) {
      if ( pOption->GetExpiry() >= GetNoon().date() ) { // skip stale dated expiries
        m_vOptions.push_back( pOption );
        m_pOptionRegistry->Add( pOption );
      }
    };

  for ( const mapChains_t::value_type& vt: m_mapChains ) {
    try {
      const std::string sCall = vt.second.GetIQFeedNameCall( m_dblStrikeCurrent );
      const std::string sPut = vt.second.GetIQFeedNamePut( m_dblStrikeCurrent );
      // ensure both strings are available prior to construction
      m_fConstructOption( sCall, fAddOption );
      m_fConstructOption( sPut,  fAddOption );
    }
    catch ( const std::runtime_error& e ) {
      // skip it, maybe emit message
    }
  }
}

void ManageStrategy::ManageIVTracker_RH() { // once a second
  if ( 0.0 < m_dblPriceCurrent ) {

    mapChains_t::const_iterator iterChains = m_mapChains.begin();
    const chain_t& chain( iterChains->second );
    const double strike = chain.Atm( m_dblPriceCurrent );

    if ( 0.0 == m_dblStrikeCurrent ) { // initial population of options
      m_dblStrikeCurrent = strike;
      ManageIVTracker_BuildRow();
    }
    else {
      if ( strike != m_dblStrikeCurrent ) {
        double diffStrike = strike - m_dblStrikeCurrent;
        if ( 0.0 > diffStrike ) diffStrike = -diffStrike;
        double diffPrice = m_dblPriceCurrent - m_dblStrikeCurrent;
        if ( 0.0 > diffPrice ) diffPrice = -diffPrice;

        if ( ( 0.66 * diffStrike ) < diffPrice ) { // hysterisis
          m_dblStrikeCurrent = strike;
          ManageIVTracker_End();
          ManageIVTracker_BuildRow();
        }
      }
    }
  }
}

void ManageStrategy::ManageIVTracker_Emit() {
  for ( vOptions_t::value_type& vt: m_vOptions ) {
    vt->EmitValues( m_dblPriceCurrent );
    std::cout << std::endl;
  }
}

void ManageStrategy::ManageIVTracker_End() {
  for ( vOptions_t::value_type& vt: m_vOptions ) {
    m_pOptionRegistry->Remove( vt, false );
  }
  m_vOptions.clear();
}

void ManageStrategy::RHEquity( const ou::tf::Bar& bar ) {
  switch ( m_stateTrading ) {
    case TSWaitForEntry:
      bool bFirstFound( false );
      double dblPrevious {};
      bool bAllRising( true );
      bool bAllFalling( true );
      std::for_each(  // calculate relative ema
        m_vEMA.begin(), m_vEMA.end(),
        [&,this]( pEMA_t& p ){
          if ( bFirstFound ) {
            double dblLatest = p->dblEmaLatest;
            bAllRising  &= dblLatest < dblPrevious;
            bAllFalling &= dblLatest > dblPrevious;
            dblPrevious = dblLatest;
          }
          else {
            dblPrevious = p->dblEmaLatest;
            bFirstFound = true;
          }
//          bAllRising  &= EMA::State::rising == p->state;
//          bAllFalling &= EMA::State::falling == p->state;
      } );
      // need three consecutive bars in the trending direction
//      bAllRising &= ( EBarDirection::Up == m_rBarDirection[ 0 ] );
//      bAllRising &= ( EBarDirection::Up == m_rBarDirection[ 1 ] );
//      bAllRising &= ( EBarDirection::Up == m_rBarDirection[ 2 ] );
//      bAllFalling &= ( EBarDirection::Down == m_rBarDirection[ 0 ] );
//      bAllFalling &= ( EBarDirection::Down == m_rBarDirection[ 1 ] );
//      bAllFalling &= ( EBarDirection::Down == m_rBarDirection[ 2 ] );
      static const size_t nConfirmationIntervalsPreload( 19 );
      if ( bAllRising && bAllFalling ) { // special message for questionable result
        std::cout << m_pWatchUnderlying->GetInstrument()->GetInstrumentName() << ": bAllRising && bAllFalling" << std::endl;
        m_stateEma = EmaState::EmaUnstable;
        m_nConfirmationIntervals = nConfirmationIntervalsPreload;
      }
      else {
        if ( !bAllRising && !bAllFalling ) {
          m_stateEma = EmaState::EmaUnstable;
          m_nConfirmationIntervals = nConfirmationIntervalsPreload;
        }
        else {
          if ( bAllRising ) {
            if ( EmaState::EmaUp == m_stateEma ) {
              m_nConfirmationIntervals--;
              if ( 0 == m_nConfirmationIntervals ) {
//                std::cout << m_pPositionUnderlying->GetInstrument()->GetInstrumentName() << " " << bar.DateTime() << ": placing long " << m_nSharesToTrade << std::endl;
//                m_pPositionUnderlying->CancelOrders();
//                m_pPositionUnderlying->PlaceOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Buy, m_nSharesToTrade );
//                m_ceLongEntries.AddLabel( bar.DateTime(), bar.Close(), "Long" );
                m_stateTrading = TSMonitorLong;
                m_stateEma = EmaState::EmaUnstable;
              }
            }
            else {
              m_stateEma = EmaState::EmaUp;
              m_nConfirmationIntervals = nConfirmationIntervalsPreload;
            }
          }
          if ( bAllFalling ) {
            if ( EmaState::EmaDown == m_stateEma ) {
              m_nConfirmationIntervals--;
              if ( 0 == m_nConfirmationIntervals ) {
//                std::cout << m_pPositionUnderlying->GetInstrument()->GetInstrumentName() << " " << bar.DateTime() << ": placing short " << m_nSharesToTrade << std::endl;
//                m_pPositionUnderlying->CancelOrders();
//                m_pPositionUnderlying->PlaceOrder( ou::tf::OrderType::Market, ou::tf::OrderSide::Sell, m_nSharesToTrade );
//                m_ceShortEntries.AddLabel( bar.DateTime(), bar.Close(), "Short" );
                m_stateTrading = TSMonitorShort;
                m_stateEma = EmaState::EmaUnstable;
              }
            }
            else {
              m_stateEma = EmaState::EmaDown;
              m_nConfirmationIntervals = nConfirmationIntervalsPreload;
            }
          }
        }
      }
      break;
  }
}

// 4 minutes prior to close
void ManageStrategy::HandleCancel( boost::gregorian::date, boost::posix_time::time_duration ) {
  switch ( m_stateTrading ) {
    case TSNoMore:
      break;
    default:
      {
        std::cout << m_pWatchUnderlying->GetInstrument()->GetInstrumentName() << " cancel" << std::endl;
        if ( m_pCombo ) {
          //if ( m_pPositionUnderlying ) m_pPositionUnderlying->CancelOrders();
          combo_t& combo = dynamic_cast<combo_t&>( *m_pCombo );
          //entry.second.ClosePositions();
          combo.CancelOrders(); // TODO: generify via Common or Base
        }
      }
      break;
  }
}

// one shot, 3 minutes, 45 seconds prior to close
void ManageStrategy::HandleGoNeutral( boost::gregorian::date date, boost::posix_time::time_duration time ) {
  switch ( m_stateTrading ) {
    case TSNoMore:
      break;
    default:
//      std::cout << m_sUnderlying << " go neutral" << std::endl;
      if ( m_pCombo ) {
        m_pCombo->GoNeutral( date, time );
      }
      break;
  }
  ManageIVTracker_End();
}

void ManageStrategy::HandleGoingNeutral( const ou::tf::Bar& bar ) {
  RHOption( bar );
}

void ManageStrategy::HandleAtRHClose( boost::gregorian::date, boost::posix_time::time_duration ) {
  switch ( m_stateTrading ) {
    case TSNoMore:
      break;
    default:
      if ( m_pCombo ) {
        m_pCombo->AtClose();      }
      break;
  }
}

void ManageStrategy::HandleAfterRH( const ou::tf::Quote& quote ) {
  switch ( m_stateTrading ) {
    case TSNoMore:
      break;
    default:
//      if ( nullptr != m_pPositionUnderlying )  // no meaning in an option only context
//        std::cout << m_sUnderlying << " close results underlying " << *m_pPositionUnderlying << std::endl;
      m_stateTrading = TSNoMore;
      break;
  }
  // TODO: need to set a state to do this once, rather than the TSNoMore kludge?
}

void ManageStrategy::HandleAfterRH( const ou::tf::Trade& trade ) {
}

void ManageStrategy::HandleAfterRH( const ou::tf::Bar& bar ) {
  //std::cout << m_sUnderlying << " close results " << *m_pPositionUnderlying << std::endl;
  // need to set a state to do this once
}

void ManageStrategy::SaveSeries( const std::string& sPrefix ) {
  // TODO: pWatchUnderlying should be saved in caller hierarchy
  if ( m_pCombo ) {
    combo_t& combo( dynamic_cast<combo_t&>( *m_pCombo ) );
    //entry.second.ClosePositions();
    combo.SaveSeries( sPrefix ); // TODO: generify via Common or Base
  }
}

void ManageStrategy::HandleBarQuotes01Sec( const ou::tf::Bar& bar ) {
  TimeTick( bar );
}

void ManageStrategy::HandleBarTrades01Sec( const ou::tf::Bar& bar ) {

  if ( 0 == m_vEMA.size() ) {  // issue here is that as vector is updated, memory is moved, using heap instead

    m_vEMA.emplace_back( std::make_shared<EMA>(  5, m_pChartDataView, ou::Colour::DarkOrange ) );
    m_vEMA.back().get()->SetName( "Ema 5s" );

    m_vEMA.emplace_back( std::make_shared<EMA>( 13, m_pChartDataView, ou::Colour::MediumTurquoise ) );
    m_vEMA.back().get()->SetName( "Ema 13s" );

    m_vEMA.emplace_back( std::make_shared<EMA>( 34, m_pChartDataView, ou::Colour::DarkOrchid ) );
    m_vEMA.back().get()->SetName( "Ema 34s" );

    m_vEMA.emplace_back( std::make_shared<EMA>( 89, m_pChartDataView, ou::Colour::DarkMagenta ) );
    m_vEMA.back().get()->SetName( "Ema 89s" );

    std::for_each(
      m_vEMA.begin(), m_vEMA.end(),
      [&bar]( pEMA_t& p ){
        p->First( bar.DateTime(), bar.Close() );
      } );
  }
  else {
    std::for_each(
      m_vEMA.begin(), m_vEMA.end(),
      [&bar]( pEMA_t& p ){
        p->Update( bar.DateTime(), bar.Close() );
      } );
  }

  //TimeTick( bar );  // using quotes as time tick

}

void ManageStrategy::HandleBarTrades06Sec( const ou::tf::Bar& bar ) {

  //m_cePrice.AppendBar( bar );
  //m_ceVolume.Append( bar );

//  m_rBarDirection[ 0 ] = m_rBarDirection[ 1 ];
//  m_rBarDirection[ 1 ] = m_rBarDirection[ 2 ];
//  m_rBarDirection[ 2 ] = ( bar.Open() == bar.Close() ) ? EBarDirection::None : ( ( bar.Open() < bar.Close() ) ? EBarDirection::Up : EBarDirection::Down );

  double dblUnRealized;
  double dblRealized;
  double dblCommissionsPaid;
  double dblTotal;

  if ( m_pCombo ) {
    pPortfolio_t pPortfolioStrategy = m_pCombo->GetPortfolio();
    if ( pPortfolioStrategy ) {
      pPortfolioStrategy->QueryStats( dblUnRealized, dblRealized, dblCommissionsPaid, dblTotal );
      m_ceProfitLossStrategy.Append( bar.DateTime(), dblTotal );
    }
  }

//  if ( m_pPositionCall ) {
//    m_pPositionCall->QueryStats( dblUnRealized, dblRealized, dblCommissionsPaid, dblTotal );
//    m_ceProfitLossCall.Append( bar.DateTime(), dblTotal );
//  }

//  if ( m_pPositionPut ) {
//    m_pPositionPut->QueryStats( dblUnRealized, dblRealized, dblCommissionsPaid, dblTotal );
//    m_ceProfitLossPut.Append( bar.DateTime(), dblTotal );
//  }

//  m_ceUpReturn.Append( bar.DateTime(), m_cntUpReturn );
//  m_cntUpReturn = 0;

//  m_ceDnReturn.Append( bar.DateTime(), m_cntDnReturn );
//  m_cntDnReturn = 0;
}

void ManageStrategy::HandleBarTicks06Sec( const ou::tf::Bar& bar ) {
  m_ceTickCount.Append( bar.DateTime(), bar.Volume() );
}

// unused without m_bfTrades60Sec
void ManageStrategy::HandleBarTrades60Sec( const ou::tf::Bar& bar ) { // sentiment event trigger for MasterPortfolio
  //m_fBar( *this, bar );
}

void ManageStrategy::Test() {
  if ( 0 != m_mapChains.size() ) {
    chain_t& chain( m_mapChains.begin()->second );
    chain.EmitValues();
    chain.Test( 121.5 );
  }
}

double ManageStrategy::EmitInfo() {
  double dblNet {};
  if ( m_pCombo ) {
    double price( m_pWatchUnderlying->LastTrade().Price() );
    combo_t& combo( dynamic_cast<combo_t&>( *m_pCombo ) );
    std::cout
      << "Info "
      << m_pWatchUnderlying->GetInstrument()->GetInstrumentName()
      << "@" << price
      << std::endl;

    std::cout << "  portfolio: " << combo.GetPortfolio()->Id() << std::endl;
    std::cout << "  underlying: ";
    m_pWatchUnderlying->EmitValues( true );
    std::cout << std::endl;

    dblNet += combo.GetNet( price );
    std::cout << "  net: " << dblNet << std::endl;
  }
  return dblNet;
}

void ManageStrategy::CloseExpiryItm( boost::gregorian::date date ) {
  double price( m_TradeUnderlyingLatest.Price() );
  if ( 0.0 != price ) {
    if ( m_pCombo ) {
      combo_t& combo( dynamic_cast<combo_t&>( *m_pCombo ) );
      combo.CloseExpiryItm( price, date );
    }
  }
}

void ManageStrategy::CloseFarItm() {
  double price( m_TradeUnderlyingLatest.Price() );
  if ( 0.0 != price ) {
    if ( m_pCombo ) {
      combo_t& combo( dynamic_cast<combo_t&>( *m_pCombo ) );
      combo.CloseFarItm( price );
    }
  }
}

void ManageStrategy::CloseItmLeg() {
  double price( m_TradeUnderlyingLatest.Price() );
  if ( 0.0 != price ) {
    if ( m_pCombo ) {
      combo_t& combo( dynamic_cast<combo_t&>( *m_pCombo ) );
      combo.CloseItmLeg( price );
    }
  }
}

void ManageStrategy::CloseForProfits() {
  double price( m_TradeUnderlyingLatest.Price() );
  if ( 0.0 != price ) {
    if ( m_pCombo ) {
      combo_t& combo( dynamic_cast<combo_t&>( *m_pCombo ) );
      combo.CloseForProfits( price );
    }
  }
}

void ManageStrategy::TakeProfits() {
  double price( m_TradeUnderlyingLatest.Price() );
  if ( 0.0 != price ) {
    if ( m_pCombo ) {
      combo_t& combo( dynamic_cast<combo_t&>( *m_pCombo ) );
      combo.TakeProfits( price );
    }
  }
}
