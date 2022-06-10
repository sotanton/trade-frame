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
 * File:    one_shot.hpp
 * Author:  raymond@burkholder.net
 * Project: lib/TFAlpaca
 * Created: June 6, 2022 15:01
 */

#include <iostream>

#include <boost/beast/version.hpp>

#include "one_shot.hpp"

namespace ou {
namespace tf {
namespace alpaca {
namespace session {

namespace { // anonymous

const static int nVersion( 11 );
const static std::string sUserAgent( "ounl.tradeframe/1.0" );
const static std::string sFieldAlpacaKeyId( "APCA-API-KEY-ID" );
const static std::string sFieldAlpacaSecret( "APCA-API-SECRET-KEY" );

// Report a failure
void fail( beast::error_code ec, char const* what ) {
  std::cerr << what << ": " << ec.message() << "\n";
}

} // namespace anonymous

one_shot::one_shot(
  asio::any_io_executor ex,
  ssl::context& ssl_ctx
)
: m_resolver( ex )
, m_stream( ex, ssl_ctx )
, m_fWriteRequest( nullptr )
, m_fDone( nullptr )
{
  std::cout << "alpaca::one_shot construction" << std::endl; // ensuring proper timing of handling
}

one_shot::~one_shot() {
  std::cout << "alpaca::one_shot destruction" << std::endl;  // ensuring proper timing of handling
}

void one_shot::run(
  const std::string& sHost,
  const std::string& sPort,
  const std::string& sTarget,
  int version,
  const std::string& sAlpacaKey,
  const std::string& sAlpacaSecret
) {
  // Set SNI Hostname (many hosts need this to handshake successfully)
  if( !SSL_set_tlsext_host_name( m_stream.native_handle(), sHost.c_str() ) )
  {
      beast::error_code ec{ static_cast<int>( ::ERR_get_error()), asio::error::get_ssl_category() };
      std::cerr << ec.message() << "\n";
      return;
  }

  // Set up an HTTP GET request message
  m_request_empty.version( version );
  m_request_empty.method( http::verb::get );
  m_request_empty.set( http::field::host, sHost );
  //request_.set( http::field::user_agent, BOOST_BEAST_VERSION_STRING );
  m_request_empty.set( http::field::user_agent, sUserAgent );

  m_request_empty.target( sTarget );
  m_request_empty.set( sFieldAlpacaKeyId, sAlpacaKey );
  m_request_empty.set( sFieldAlpacaSecret, sAlpacaSecret );
  //req_.body() = json::serialize( jv );
  //req_.prepare_payload();

  m_fWriteRequest = [this](){ write_empty(); };
  m_fDone = []( bool, const std::string& ){};

  // Look up the domain name
  m_resolver.async_resolve(
    sHost, sPort,
    beast::bind_front_handler(
      &one_shot::on_resolve,
      shared_from_this()
    )
  );
}

void one_shot::get(
    const std::string& sHost,
    const std::string& sPort,
    const std::string& sAlpacaKey,
    const std::string& sAlpacaSecret,
    const std::string& sTarget,
    fDone_t&& fDone
) {

  m_fDone = std::move( fDone );
  assert( m_fDone );

  // Set SNI Hostname (many hosts need this to handshake successfully)
  if( !SSL_set_tlsext_host_name( m_stream.native_handle(), sHost.c_str() ) )
  {
      beast::error_code ec{ static_cast<int>( ::ERR_get_error()), asio::error::get_ssl_category() };
      std::cerr << ec.message() << "\n";
      m_fDone( false, ec.message() );
      return;
  }

  // Set up an HTTP GET request message
  m_request_empty.version( nVersion );
  m_request_empty.method( http::verb::get );
  m_request_empty.set( http::field::host, sHost );
  //request_.set( http::field::user_agent, BOOST_BEAST_VERSION_STRING );
  m_request_empty.set( http::field::user_agent, sUserAgent );

  m_request_empty.target( sTarget );
  m_request_empty.set( sFieldAlpacaKeyId, sAlpacaKey );
  m_request_empty.set( sFieldAlpacaSecret, sAlpacaSecret );

  m_fWriteRequest = [this](){ write_empty(); };

  // Look up the domain name
  m_resolver.async_resolve(
    sHost, sPort,
    beast::bind_front_handler(
      &one_shot::on_resolve,
      shared_from_this()
    )
  );
}

void one_shot::post(
    const std::string& sHost,
    const std::string& sPort,
    const std::string& sAlpacaKey,
    const std::string& sAlpacaSecret,
    const std::string& sTarget,
    const std::string& sBody,
    fDone_t&& fDone
) {

  m_fDone = std::move( fDone );
  assert( m_fDone );

  // Set SNI Hostname (many hosts need this to handshake successfully)
  if( !SSL_set_tlsext_host_name( m_stream.native_handle(), sHost.c_str() ) )
  {
      beast::error_code ec{ static_cast<int>( ::ERR_get_error()), asio::error::get_ssl_category() };
      std::cerr << ec.message() << "\n";
      m_fDone( false, ec.message() );
      return;
  }

  // Set up an HTTP GET request message
  m_request_body.version( nVersion );
  m_request_body.method( http::verb::post );
  m_request_body.set( http::field::host, sHost );
  //request_.set( http::field::user_agent, BOOST_BEAST_VERSION_STRING );
  m_request_body.set( http::field::user_agent, sUserAgent );

  m_request_body.target( sTarget );
  m_request_body.set( sFieldAlpacaKeyId, sAlpacaKey );
  m_request_body.set( sFieldAlpacaSecret, sAlpacaSecret );

  m_request_body.body() = sBody;

  m_fWriteRequest = [this](){ write_body(); };

  // Look up the domain name
  m_resolver.async_resolve(
    sHost, sPort,
    beast::bind_front_handler(
      &one_shot::on_resolve,
      shared_from_this()
    )
  );
}

void one_shot::on_resolve(
  beast::error_code ec,
  tcp::resolver::results_type results
) {
  if ( ec ) {
    fail( ec, "resolve");
    m_fDone( false, "resolve" );
  }
  else {
    // Set a timeout on the operation
    beast::get_lowest_layer( m_stream ).expires_after( std::chrono::seconds( 15 ) );

    // Make the connection on the IP address we get from a lookup
    beast::get_lowest_layer( m_stream ).async_connect(
      results,
      beast::bind_front_handler(
        &one_shot::on_connect,
        shared_from_this() )
      );
  }
}

void one_shot::on_connect( beast::error_code ec, tcp::resolver::results_type::endpoint_type et ) {
  if ( ec ) {
    fail( ec, "connect" );
    m_fDone( false, "connect" );
  }
  else {
    // Perform the SSL handshake
    m_stream.async_handshake(
      ssl::stream_base::client,
      beast::bind_front_handler(
        &one_shot::on_handshake,
        shared_from_this()
      )
    );
  }
}

void one_shot::on_handshake( beast::error_code ec ) {

  if ( ec ) {
    fail( ec, "handshake" );
    m_fDone( false, "handshake" );
  }
  else {
    // Set a timeout on the operation
    beast::get_lowest_layer( m_stream ).expires_after( std::chrono::seconds( 15 ) );

    assert( m_fWriteRequest );
    m_fWriteRequest();
  }
}

void one_shot::write_empty() {
  // Send the HTTP request to the remote host
  http::async_write( m_stream, m_request_empty,
    beast::bind_front_handler(
      &one_shot::on_write,
      shared_from_this()
    )
  );
}

void one_shot::write_body() {
  // Send the HTTP request to the remote host
  http::async_write( m_stream, m_request_body,
    beast::bind_front_handler(
      &one_shot::on_write,
      shared_from_this()
    )
  );
}

void one_shot::on_write(
  beast::error_code ec,
  std::size_t bytes_transferred
) {
  boost::ignore_unused(bytes_transferred);

  if ( ec ) {
    fail( ec, "write" );
    m_fDone( false, "write" );
  }
  else {
    // Receive the HTTP response
    http::async_read(
      m_stream, m_buffer, m_response,
      beast::bind_front_handler(
        &one_shot::on_read,
        shared_from_this()
      )
    );
  }
}

void one_shot::on_read( beast::error_code ec, std::size_t bytes_transferred ) {

  boost::ignore_unused( bytes_transferred );

  if ( ec ) {
    fail( ec, "read" );
    m_fDone( false, "read" );
  }
  else {
    m_fDone( true, m_response.body() );
    // Set a timeout on the operation
    beast::get_lowest_layer( m_stream ).expires_after( std::chrono::seconds( 15 ) );

    // Gracefully close the stream - can the stream be re-used?
    m_stream.async_shutdown(
      beast::bind_front_handler(
        &one_shot::on_shutdown,
        shared_from_this()
      )
    );
  }
}

void one_shot::on_shutdown( beast::error_code ec ) {
  if ( ec == asio::error::eof ) {
      // Rationale:
      // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
      ec = {};
  }
  if(ec)
    return fail( ec, "shutdown" );

  // If we get here then the connection is closed gracefully
}

} // namespace session
} // namespace alpaca
} // namespace tf
} // namespace ou
