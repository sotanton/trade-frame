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
 * File:    InteractiveChart.h
 * Author:  raymond@burkholder.net
 * Project: IndicatorTrading
 * Created: February 8, 2022 13:38
 */

#pragma once

#include <OUCommon/Colour.h>
#include <functional>

#include <boost/serialization/version.hpp>
#include <boost/serialization/split_member.hpp>

#include <OUCharting/ChartDataView.h>

#include <OUCharting/ChartEntryBars.h>
#include <OUCharting/ChartEntryMark.h>
#include <OUCharting/ChartEntryShape.h>
#include <OUCharting/ChartEntryVolume.h>
#include <OUCharting/ChartEntryIndicator.h>

#include <TFIndicators/TSEMA.h>
#include <TFIndicators/TSSWStochastic.h>

#include <TFTimeSeries/BarFactory.h>

#include <TFIQFeed/OptionChainQuery.h>

#include <TFTrading/Order.h>
#include <TFTrading/Position.h>

#include <TFOptions/Chain.h>
#include <TFOptions/Chains.h>
#include <TFOptions/Option.h>

#include <TFVuTrading/WinChartView.h>

namespace ou { // One Unified
namespace tf { // TradeFrame
namespace iqfeed { // IQFeed
  class OptionChainQuery;
} // namespace iqfeed
namespace option {
  class Engine;
} // namespace option
  struct PanelOrderButtons_Order;
  struct PanelOrderButtons_MarketData;
} // namespace tf
} // namespace ou

namespace config {
  class Options;
}

class TradeLifeTime;

class InteractiveChart:
  public ou::tf::WinChartView
{
public:

  using idOrder_t = ou::tf::Order::idOrder_t;

  using pOrder_t = ou::tf::Order::pOrder_t;
  using pOption_t = ou::tf::option::Option::pOption_t;
  using pPosition_t = ou::tf::Position::pPosition_t;

  using fOption_t = std::function<void(pOption_t)>;
  using fBuildOption_t = std::function<void(const std::string&,fOption_t&&)>;


  using fUpdateLifeCycle_t = std::function<void(const std::string&)>;
  using fDeleteLifeCycle_t = std::function<void()>;

  struct LifeCycleFunctions {
    fUpdateLifeCycle_t fUpdateLifeCycle;
    fDeleteLifeCycle_t fDeleteLifeCycle;
    LifeCycleFunctions( fUpdateLifeCycle_t&& fUpdateLifeCycle_,fDeleteLifeCycle_t&& fDeleteLifeCycle_ )
    : fUpdateLifeCycle( std::move( fUpdateLifeCycle_ ) ), fDeleteLifeCycle( std::move( fDeleteLifeCycle_ ) )
    {}
  };

  using fOnClick_t = std::function<void()>;
  using fAddLifeCycleToTree_t = std::function<LifeCycleFunctions(idOrder_t)>;
  using fAddOptionToTree_t = std::function<void( const std::string&, fOnClick_t&& )>;
  using fAddExpiryToTree_t = std::function<fAddOptionToTree_t( const std::string& )>;

  struct SubTreesForUnderlying {
    fAddLifeCycleToTree_t fAddLifeCycleToTree;
    fAddExpiryToTree_t fAddExpiryToTree;
    SubTreesForUnderlying( fAddLifeCycleToTree_t&& fAddLifeCycleToTree_, fAddExpiryToTree_t&& fAddExpiryToTree_)
    : fAddLifeCycleToTree( std::move( fAddLifeCycleToTree_ ) ), fAddExpiryToTree( std::move( fAddExpiryToTree_ ) )
    {}
  };

  using fAddUnderlying_t = std::function<SubTreesForUnderlying(const std::string&, fOnClick_t&&)>; // primary start for tree hierarchy

  using pOptionChainQuery_t = std::shared_ptr<ou::tf::iqfeed::OptionChainQuery>;

  InteractiveChart();
  InteractiveChart(
    wxWindow* parent,
    wxWindowID id = SYMBOL_WIN_CHARTINTERACTIVE_IDNAME,
    const wxPoint& pos = SYMBOL_WIN_CHARTINTERACTIVE_POSITION,
    const wxSize& size = SYMBOL_WIN_CHARTINTERACTIVE_SIZE,
    long style = SYMBOL_WIN_CHARTINTERACTIVE_STYLE );

  bool Create(
    wxWindow* parent,
    wxWindowID id = SYMBOL_WIN_CHARTINTERACTIVE_IDNAME,
    const wxPoint& pos = SYMBOL_WIN_CHARTINTERACTIVE_POSITION,
    const wxSize& size = SYMBOL_WIN_CHARTINTERACTIVE_SIZE,
    long style = SYMBOL_WIN_CHARTINTERACTIVE_STYLE );

  virtual ~InteractiveChart();

  void SetPosition(
     pPosition_t
   , const config::Options&
   , pOptionChainQuery_t
   , fBuildOption_t&&
   , fAddUnderlying_t&&
    );

  void EmitChainFull() const {
    size_t cnt {};
    std::cout << "underlying: " << m_pPosition->GetInstrument()->GetInstrumentName( ou::tf::Instrument::eidProvider_t::EProviderIQF ) << std::endl;
    for ( const mapChains_t::value_type& vt: m_mapChains ) {
      std::cout << "chain: " << vt.first << " has " << vt.second.Size() << " entries" << std::endl;
      cnt += vt.second.EmitValues();
      //vt.second.EmitSummary();
    }
    std::cout << "EmitChainFull total chain strikes=" << cnt << std::endl;
  }

  void EmitChainSummary() const {
    size_t cnt {};
    std::cout << "underlying: " << m_pPosition->GetInstrument()->GetInstrumentName( ou::tf::Instrument::eidProvider_t::EProviderIQF ) << std::endl;
    for ( const mapChains_t::value_type& vt: m_mapChains ) {
      std::cout << "chain: " << vt.first << " has " << vt.second.Size() << " entries" << std::endl;
      //vt.second.EmitValues();
      cnt += vt.second.EmitSummary();
    }
    std::cout << "EmitChainSummary total sum(call + put)=" << cnt << std::endl;
  }

  void ProcessChains();

  void SaveWatch( const std::string& );

  void OptionWatchStart();
  void OptionQuoteShow();
  void OptionWatchStop();
  void OptionEmit();

  void OrderBuy( const ou::tf::PanelOrderButtons_Order& );
  void OrderSell( const ou::tf::PanelOrderButtons_Order& );
  void OrderClose( const ou::tf::PanelOrderButtons_Order& );
  void OrderCancel( const ou::tf::PanelOrderButtons_Order& );

  void CancelOrders();

  void OrderCancel( idOrder_t );
  void EmitOrderStatus( idOrder_t );
  void DeleteLifeCycle( idOrder_t );

  void EmitStatus();

  void Connect();
  void Disconnect();

protected:
private:

  enum EChartSlot { Price, Volume, StochInd, Sentiment, PL, Spread }; // IndMA = moving averate indicator

  using pWatch_t = ou::tf::Watch::pWatch_t;
  using pChartDataView_t = ou::ChartDataView::pChartDataView_t;

  bool m_bConnected;
  bool m_bOptionsReady;

  ou::ChartDataView m_dvChart; // the data

  pOrder_t m_pOrder;
  pPosition_t m_pPosition;

  double m_dblSumVolume; // part of vwap
  double m_dblSumVolumePrice; // part of vwap

  ou::tf::BarFactory m_bfPrice;
  ou::tf::BarFactory m_bfPriceUp;
  ou::tf::BarFactory m_bfPriceDn;

  ou::ChartEntryIndicator m_ceTrade;
  ou::ChartEntryBars m_cePriceBars;

  //ou::ChartEntryIndicator m_ceVWAP;

  ou::ChartEntryVolume m_ceVolume;
  ou::ChartEntryVolume m_ceVolumeUp;
  ou::ChartEntryVolume m_ceVolumeDn;

  ou::ChartEntryIndicator m_ceQuoteAsk;
  ou::ChartEntryIndicator m_ceQuoteBid;
  ou::ChartEntryIndicator m_ceQuoteSpread;

  ou::ChartEntryShape m_ceBuySubmit;
  ou::ChartEntryShape m_ceBuyFill;
  ou::ChartEntryShape m_ceSellSubmit;
  ou::ChartEntryShape m_ceSellFill;
  ou::ChartEntryShape m_ceCancelled;

  ou::ChartEntryShape m_ceBullCall;
  ou::ChartEntryShape m_ceBullPut;
  ou::ChartEntryShape m_ceBearCall;
  ou::ChartEntryShape m_ceBearPut;

  ou::ChartEntryIndicator m_ceProfitLoss;

  ou::ChartEntryMark m_cemStochastic;

  ou::tf::Quote m_quote;

  struct Stochastic {

    using pTSSWStochastic_t = std::unique_ptr<ou::tf::TSSWStochastic>;

    pTSSWStochastic_t m_pIndicatorStochastic;
    ou::ChartEntryIndicator m_ceStochastic;
    ou::ChartEntryIndicator m_ceStochasticMax;
    ou::ChartEntryIndicator m_ceStochasticMin;

    Stochastic( const std::string sIx, ou::tf::Quotes& quotes, int nPeriods, time_duration td, ou::Colour::enumColour colour ) {

      m_ceStochastic.SetColour( colour );
      m_ceStochasticMax.SetColour( colour );
      m_ceStochasticMin.SetColour( colour );

      m_ceStochastic.SetName( "Stoch" + sIx );
      m_ceStochasticMax.SetName( "Stoch" + sIx + " Max" );
      m_ceStochasticMin.SetName( "Stoch" + sIx + " Min" );

      m_pIndicatorStochastic = std::make_unique<ou::tf::TSSWStochastic>(
        quotes, nPeriods, td,
        [this,sIx]( ptime dt, double k, double min, double max ){
          //std::cout << sIx << " is " << k << "," << max << "," << min << std::endl;
          m_ceStochastic.Append( dt, k );
          m_ceStochasticMax.Append( dt, max );
          m_ceStochasticMin.Append( dt, min );
        }
      );
    }

    Stochastic( const Stochastic& ) = delete;
    Stochastic( Stochastic&& rhs ) = delete;

    void AddToChart( ou::ChartDataView& cdv ) {
      cdv.Add( EChartSlot::Price, &m_ceStochasticMax );
      cdv.Add( EChartSlot::Price, &m_ceStochasticMin );
      cdv.Add( EChartSlot::StochInd, &m_ceStochastic );
    }

    ~Stochastic() {
      m_pIndicatorStochastic.reset();
      m_ceStochastic.Clear();
      m_ceStochasticMax.Clear();
      m_ceStochasticMin.Clear();
    }

  };

  using pStochastic_t = std::unique_ptr<Stochastic>;
  using vStochastic_t = std::vector<pStochastic_t>;
  vStochastic_t m_vStochastic;

  struct MA {

    ou::tf::hf::TSEMA<ou::tf::Quote> m_ema;
    ou::ChartEntryIndicator m_ceMA;

    MA( ou::tf::Quotes& quotes, size_t nPeriods, time_duration tdPeriod, ou::Colour::enumColour colour, const std::string& sName )
    : m_ema( quotes, nPeriods, tdPeriod )
    {
      m_ceMA.SetName( sName );
      m_ceMA.SetColour( colour );
    }

    MA( MA&& rhs )
    : m_ema(  std::move( rhs.m_ema ) )
    , m_ceMA( std::move( rhs.m_ceMA ) )
    {}

    void AddToView( ou::ChartDataView& cdv ) {
      cdv.Add( EChartSlot::Price, &m_ceMA );
    }

    void AddToView( ou::ChartDataView& cdv, EChartSlot slot ) {
      cdv.Add( slot, &m_ceMA );
    }

    void Update( ptime dt ) {
      m_ceMA.Append( dt, m_ema.GetEMA() );
    }

    double Latest() const { return m_ema.GetEMA(); }
  };

  using vMA_t = std::vector<MA>;
  vMA_t m_vMA;

  fBuildOption_t m_fBuildOption;
  fAddLifeCycleToTree_t m_fAddLifeCycleToTree;
  fAddExpiryToTree_t m_fAddExpiryToTree;

  struct BuiltOption: public ou::tf::option::chain::OptionName {
    pOption_t pOption;
  };

  using chain_t = ou::tf::option::Chain<BuiltOption>;
  using mapChains_t = std::map<boost::gregorian::date, chain_t>;
  mapChains_t m_mapChains;

  pOptionChainQuery_t m_pOptionChainQuery; // need to disconnect

  struct Expiry {
    fAddOptionToTree_t fAddOptionToTree;
  };
  using mapExpiries_t = std::map<boost::gregorian::date,Expiry>; // usable chains
  mapExpiries_t m_mapExpiries; // possibly change this to a map of iterators

  // ==

  struct OptionTracker {

    bool m_bActive;
    pOption_t m_pOption;
    ou::tf::Trade::volume_t m_volBuy;
    ou::tf::Trade::volume_t m_volSell;

    ou::ChartEntryShape& m_ceBullCall;
    ou::ChartEntryShape& m_ceBullPut;
    ou::ChartEntryShape& m_ceBearCall;
    ou::ChartEntryShape& m_ceBearPut;

    ou::ChartEntryIndicator m_ceTrade;

    ou::ChartEntryVolume m_ceVolumeUp;
    ou::ChartEntryVolume m_ceVolumeDn;

    ou::ChartEntryIndicator m_ceQuoteAsk;
    ou::ChartEntryIndicator m_ceQuoteBid;

    ou::ChartDataView m_dvChart; // the data

    void Add() {

      if ( !m_bActive ) {
        m_bActive = true;
        m_pOption->OnQuote.Add( MakeDelegate( this, &OptionTracker::HandleQuote ) );
        switch ( m_pOption->GetOptionSide() ) {
          case ou::tf::OptionSide::Call:
            m_pOption->OnTrade.Add( MakeDelegate( this, &OptionTracker::HandleTradeCall ) );
            break;
          case ou::tf::OptionSide::Put:
            m_pOption->OnTrade.Add( MakeDelegate( this, &OptionTracker::HandleTradePut ) );
            break;
          default:
            assert( false );
            break;
        }

        m_ceTrade.SetColour( ou::Colour::Green );
        m_ceTrade.SetName( "Tick" );
        m_ceQuoteAsk.SetColour( ou::Colour::Red );
        m_ceQuoteAsk.SetName( "Ask" );
        m_ceQuoteBid.SetColour( ou::Colour::Blue );
        m_ceQuoteBid.SetName( "Bid" );
        m_ceVolumeUp.SetColour( ou::Colour::Green );
        m_ceVolumeUp.SetName( "Buy" );
        m_ceVolumeDn.SetColour( ou::Colour::Red );
        m_ceVolumeDn.SetName( "Sell" );

        m_dvChart.Add( 0, &m_ceQuoteAsk );
        m_dvChart.Add( 0, &m_ceQuoteBid );
        m_dvChart.Add( 0, &m_ceTrade );
        m_dvChart.Add( 1, &m_ceVolumeUp );
        m_dvChart.Add( 1, &m_ceVolumeDn );

        m_pOption->StartWatch();
      }
    }

    void Del() {
      if ( m_bActive ) {
        m_pOption->StopWatch();
        m_pOption->OnQuote.Remove( MakeDelegate( this, &OptionTracker::HandleQuote ) );
        switch ( m_pOption->GetOptionSide() ) {
          case ou::tf::OptionSide::Call:
            m_pOption->OnTrade.Remove( MakeDelegate( this, &OptionTracker::HandleTradeCall ) );
            break;
          case ou::tf::OptionSide::Put:
            m_pOption->OnTrade.Remove( MakeDelegate( this, &OptionTracker::HandleTradePut ) );
            break;
          default:
            assert( false );
            break;
        }
        m_bActive = false;
      }
    }

    OptionTracker(
      pOption_t pOption_
    , ou::ChartEntryShape& ceBullCall, ou::ChartEntryShape& ceBullPut
    , ou::ChartEntryShape& ceBearCall, ou::ChartEntryShape& ceBearPut
    )
    : m_bActive( false ), m_pOption( pOption_ )
    , m_volBuy {}, m_volSell {}
    , m_ceBullCall( ceBullCall ), m_ceBullPut( ceBullPut )
    , m_ceBearCall( ceBearCall ), m_ceBearPut( ceBearPut )
    {
      Add();
      std::cout << "option " << m_pOption->GetInstrumentName() << " added" << std::endl;
    }

    OptionTracker( const OptionTracker& rhs )
    : m_bActive( false ), m_pOption( rhs.m_pOption )
    , m_volBuy( rhs.m_volBuy ), m_volSell( rhs.m_volSell)
    , m_ceBullCall( rhs.m_ceBullCall ), m_ceBullPut( rhs.m_ceBullPut )
    , m_ceBearCall( rhs.m_ceBearCall ), m_ceBearPut( rhs.m_ceBearPut )
    {
      Add();
    }

    OptionTracker( OptionTracker&& rhs )
    : m_bActive( false )
    , m_volBuy( rhs.m_volBuy ), m_volSell( rhs.m_volSell)
    , m_ceBullCall( rhs.m_ceBullCall ), m_ceBullPut( rhs.m_ceBullPut )
    , m_ceBearCall( rhs.m_ceBearCall ), m_ceBearPut( rhs.m_ceBearPut )
    {
      rhs.Del();
      m_pOption = std::move( rhs.m_pOption );
      Add();
    };

    ~OptionTracker() {
      Del();
      m_pOption.reset();
    }

    void HandleQuote( const ou::tf::Quote& quote ) {
      m_ceQuoteAsk.Append( quote.DateTime(), quote.Ask() );
      m_ceQuoteBid.Append( quote.DateTime(), quote.Bid() );
    }

    void HandleTradeCall( const ou::tf::Trade& trade ) {
      const double price = trade.Price();
      const ou::tf::Quote& quote( m_pOption->LastQuote() );
      const double mid = quote.Midpoint();
      m_ceTrade.Append( trade.DateTime(), price );
      if ( ( price == mid ) || ( quote.Bid() == quote.Ask() ) ) {
        // can't really say, will need to check if bid came to ask or ask came to bid
      }
      else {
        ou::tf::Trade::volume_t volume = trade.Volume();
        if ( price > mid ) {
          m_volBuy += volume;
          m_ceBullCall.AddLabel( trade.DateTime(), m_pOption->GetStrike(), "C" );
          m_ceVolumeUp.Append( trade.DateTime(), volume );
          m_ceVolumeDn.Append( trade.DateTime(), 0 );
        }
        else {
          m_volSell += volume;
          m_ceBearCall.AddLabel( trade.DateTime(), m_pOption->GetStrike(), "C" );
          m_ceVolumeUp.Append( trade.DateTime(), 0 );
          m_ceVolumeDn.Append( trade.DateTime(), volume );
        }
      }
    }

    void HandleTradePut( const ou::tf::Trade& trade ) {
      const double price = trade.Price();
      const ou::tf::Quote& quote( m_pOption->LastQuote() );
      const double mid = quote.Midpoint();
      m_ceTrade.Append( trade.DateTime(), price );
      if ( ( price == mid ) || ( quote.Bid() == quote.Ask() ) ) {
        // can't really say, will need to check if bid came to ask or ask came to bid
      }
      else {
        ou::tf::Trade::volume_t volume = trade.Volume();
        if ( price > mid ) {
          m_volBuy += volume;
          m_ceBearPut.AddLabel( trade.DateTime(), m_pOption->GetStrike(), "P" );
          m_ceVolumeUp.Append( trade.DateTime(), volume );
          m_ceVolumeDn.Append( trade.DateTime(), 0 );
        }
        else {
          m_volSell += volume;
          m_ceBullPut.AddLabel( trade.DateTime(), m_pOption->GetStrike(), "P" );
          m_ceVolumeUp.Append( trade.DateTime(), 0 );
          m_ceVolumeDn.Append( trade.DateTime(), volume );
        }
      }
    }

    void SaveWatch( const std::string& sPrefix ) {
      m_pOption->SaveSeries( sPrefix );
    }

    void Emit() {
      ou::tf::Quote quote( m_pOption->LastQuote() );
      std::cout <<
           m_pOption->GetInstrumentName()
        << ": b=" << quote.Bid()
        << ",a=" << quote.Ask()
        //<< ",oi=" << m_pOption->GetFundamentals().nOpenInterest // not available
        << ",bv=" << m_volBuy
        << ",sv=" << m_volSell
        << std::endl;
    }

    ou::ChartDataView* GetDataViewChart() { return &m_dvChart; }

    template<typename Archive>
    void save( Archive& ar, const unsigned int version ) const {
      ar & m_volBuy;
      ar & m_volSell;
    }

    template<typename Archive>
    void load( Archive& ar, const unsigned int version ) {
      ar & m_volBuy;
      ar & m_volSell;
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER()

  };

  // ==

  using pOptionTracker_t = std::shared_ptr<OptionTracker>;
  using mapOptionTracker_t = std::map<std::string, pOptionTracker_t>; // map<option name,tracker>
  using mapStrikes_t = std::map<double,mapOptionTracker_t>; // map of options across strikes
  mapStrikes_t m_mapStrikes;

  using vOptionForQuote_t = std::vector<pOption_t>;
  vOptionForQuote_t m_vOptionForQuote;

  using query_t = ou::tf::iqfeed::OptionChainQuery;

  using pTradeLifeTime_t = std::shared_ptr<TradeLifeTime>;

  struct LifeCycle {
    pTradeLifeTime_t pTradeLifeTime;
    fDeleteLifeCycle_t fDeleteLifeCycle;
    LifeCycle( pTradeLifeTime_t pTradeLifeTime_ )
    : pTradeLifeTime( pTradeLifeTime_ )
    {}
  };

  using mapLifeCycle_t = std::map<idOrder_t,LifeCycle>;
  mapLifeCycle_t m_mapLifeCycle;

  void Init();

  void BindEvents();
  void UnBindEvents();

  void OnKey( wxKeyEvent& );
  void OnChar( wxKeyEvent& );
  void OnDestroy( wxWindowDestroyEvent& );

  void HandleQuote( const ou::tf::Quote& );
  void HandleTrade( const ou::tf::Trade& );

  void HandleOptionWatchQuote( const ou::tf::Quote& ) {}

  void HandleBarCompletionPrice( const ou::tf::Bar& );
  void HandleBarCompletionPriceUp( const ou::tf::Bar& );
  void HandleBarCompletionPriceDn( const ou::tf::Bar& );

  void OptionChainQuery( const std::string& );
  void PopulateChains( const query_t::OptionList& );

  void CheckOptions();
  pOptionTracker_t AddOptionTracker( double strike, pOption_t );

  template<typename Archive>
  void save( Archive& ar, const unsigned int version ) const {
  }

  template<typename Archive>
  void load( Archive& ar, const unsigned int version ) {
  }

  BOOST_SERIALIZATION_SPLIT_MEMBER()

};

BOOST_CLASS_VERSION(InteractiveChart, 1)