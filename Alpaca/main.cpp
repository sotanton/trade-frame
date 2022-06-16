//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: HTTP SSL client, asynchronous
//
//------------------------------------------------------------------------------

#include <memory>
#include <string>
#include <vector>
#include <cstdlib>
#include <iostream>
#include <functional>

#include <boost/asio/strand.hpp>

#include <TFTrading/DBWrapper.h>
#include <TFTrading/OrderManager.h>
#include <TFTrading/PortfolioManager.h>
#include <TFTrading/InstrumentManager.h>

#include <TFAlpaca/Provider.hpp>

#include "Config.hpp"

int main( int argc, char** argv )
{
    // Check command line arguments.
//    if(argc != 4 && argc != 5)
//    {
//        std::cerr <<
//            "Usage: http-client-async-ssl <host> <port> <target> [<HTTP version: 1.0 or 1.1(default)>]\n" <<
//            "Example:\n" <<
//            "    http-client-async-ssl www.example.com 443 /\n" <<
//            "    http-client-async-ssl www.example.com 443 / 1.0\n";
//        return EXIT_FAILURE;
//    }
    //auto const host = argv[1];
    //auto const port = argv[2];
    //auto const target = argv[3];

    //const std::string sVersion( "1.0" );
    //int version = ( argc == 5 && !std::strcmp( "1.0", sVersion.c_str() ) ) ? 10 : 11;
    static const int version = 11;

    static const std::string sDbName( "alpaca.db" );
    std::unique_ptr<ou::tf::db> m_pdb = std::make_unique<ou::tf::db>( sDbName );

    config::Choices choices;
    config::Load( "alpaca.cfg", choices );

    const std::string sPort( "443" );
    //const std::string sTarget( "/v2/assets?status=active&asset_class=crypto" );

    auto pProvider = ou::tf::alpaca::Provider::Factory(
      choices.m_sAlpacaDomain, choices.m_sAlpacaKey, choices.m_sAlpacaSecret
    );
    pProvider->Connect();

    sleep( 4 );

    const std::string sSymbol( "GLD" );
    const std::string sPortfolio( "USD" );

    using pInstrument_t = ou::tf::Instrument::pInstrument_t;
    ou::tf::Instrument::pInstrument_t pInstrument;

    ou::tf::InstrumentManager& im( ou::tf::InstrumentManager::GlobalInstance() );
    if ( im.Exists( sSymbol ) ) {
      pInstrument = im.Get( sSymbol );
    }
    else {
      pInstrument = std::make_shared<ou::tf::Instrument>( sSymbol, ou::tf::InstrumentType::Stock, "alpaca" );
      ou::tf::InstrumentManager::Instance().Register( pInstrument );
    }

    ou::tf::PortfolioManager& pm( ou::tf::PortfolioManager::GlobalInstance() );
    ou::tf::Position::pPosition_t pPosition;
    if ( pm.PositionExists( sPortfolio, sSymbol ) ) {
      pPosition = pm.GetPosition( sPortfolio, sSymbol );
    }
    else {
      pPosition = pm.ConstructPosition(
        sPortfolio, sSymbol, "manual",
        "alpaca", "alpaca", pProvider, pProvider, pInstrument
      );
    }

    ou::tf::OrderManager& om( ou::tf::OrderManager::GlobalInstance() );
    om.CheckOrderId( 28 ); // put this into state file

    auto pOrder = pPosition->ConstructOrder(
      ou::tf::OrderType::Market,
      ou::tf::OrderSide::Buy,
      100
    );

    om.PlaceOrder( pProvider.get(), pOrder );

    sleep( 20 );

    return EXIT_SUCCESS;
}