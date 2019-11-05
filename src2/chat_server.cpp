//
// chat_server.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <utility>
#include "asio.hpp"
#include "chat_message.hpp"
#include "Deck.hpp"
#include "Hand.hpp"



using asio::ip::tcp;

//----------------------------------------------------------------------

typedef std::deque<chat_message> chat_message_queue;



//------------------------------------------------------------------------------------------------

// deck and hand are global variables so all classes can access
Deck d;
Hand h;

//------------------------------------------------------------------------------------------------




class chat_participant
{
public:
  virtual ~chat_participant() {}
  virtual void deliver(const chat_message& msg) = 0;
  int id = 0;
private:
 
  
};

typedef std::shared_ptr<chat_participant> chat_participant_ptr;

//----------------------------------------------------------------------

class chat_room
{
public:
	// puts client in participants vector and sends past msg logs
  void join(chat_participant_ptr participant) 
  {
    participants_.insert(participant);
    for (auto msg: recent_msgs_)
      participant->deliver(msg);
    
  }

  void leave(chat_participant_ptr participant)
  {
    participants_.erase(participant);
  }

  void deliver(const chat_message& msg)
  {
    recent_msgs_.push_back(msg);
    while (recent_msgs_.size() > max_recent_msgs)
      recent_msgs_.pop_front();

    for (auto participant: participants_)
        participant->deliver(msg);
    
  }

  // TODO deliver msg to specific client
  void deliver2(const chat_message& msg, int recipient_id)
  {
    recent_msgs_.push_back(msg);
    while (recent_msgs_.size() > max_recent_msgs)
      recent_msgs_.pop_front();

    for (auto participant: participants_)
    {
      if(participant->id == recipient_id)
        participant->deliver(msg);
    }
  }


private:
  std::set<chat_participant_ptr> participants_;
  enum { max_recent_msgs = 100 };
  chat_message_queue recent_msgs_;
};

//----------------------------------------------------------------------

class chat_session
  : public chat_participant,
    public std::enable_shared_from_this<chat_session>
{
public:
  chat_session(tcp::socket socket, chat_room& room)
    : socket_(std::move(socket)),
      room_(room)
  {
  }

  void start() // client joins the chat room
  {
    room_.join(shared_from_this());
    do_read_header();
  }

  void deliver(const chat_message& msg) // send saved past msg log
  {
    bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress)
    {
      do_write();
    }
  }


private:
	// calls data() and waits for an async_write from client's do_write then calls decode
  void do_read_header() 
  {
    auto self(shared_from_this());
    asio::async_read(socket_,
        asio::buffer(read_msg_.data(), chat_message::header_length),
        [this, self](std::error_code ec, std::size_t /*length*/)
        {
          if (!ec && read_msg_.decode_header()) 
          {
          	// if player wants to hit
            if( read_msg_.ca.hit == true)
            {
              std::cout << "Player hits" << std::endl;
              Card temp = d.getCard();
              read_msg_.card = temp;
            }
            // if player wants to stand
            if ( read_msg_.ca.stand == true)
            {
            	std::cout << "Player stands" << std::endl;
            }
            
           do_read_body(); 
          }
          else
          {
            room_.leave(shared_from_this());
          }
        });
  }

  void do_read_body()
  {
    auto self(shared_from_this());
    asio::async_read(socket_,
        asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this, self](std::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
          	//code can go here too to send back to client
            
            read_msg_.encode_header(); // save classes to msg data_ array to be ready to send
            room_.deliver(read_msg_); // deliver msg to all clients
            do_read_header();
          }
          else
          {
            room_.leave(shared_from_this());
          }
        });
  }

  void do_write()
  {
    auto self(shared_from_this());
    asio::async_write(socket_,
        asio::buffer(write_msgs_.front().data(),
          write_msgs_.front().length()),
        [this, self](std::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            write_msgs_.pop_front();
            if (!write_msgs_.empty())
            {
              do_write();
            }
          }
          else
          {
            room_.leave(shared_from_this());
          }
        });
  }

  tcp::socket socket_;
  chat_room& room_;
  chat_message read_msg_;
  chat_message_queue write_msgs_;

};

//----------------------------------------------------------------------

class chat_server
{
public:
  chat_server(asio::io_context& io_context,
      const tcp::endpoint& endpoint)
    : acceptor_(io_context, endpoint)
  {
    do_accept(); 
  }

private:
  void do_accept() // accepts client's do_connect() call
  {
    acceptor_.async_accept( 
        [this](std::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
          	// start the chat_session and calls start()
            std::make_shared<chat_session>(std::move(socket), room_)->start(); 
          }
          // waiting for more clients
          do_accept(); 
        });
  }

  tcp::acceptor acceptor_;
  chat_room room_;
};

//----------------------------------------------------------------------

int main(int argc, char* argv[])
{ 
  try
  {
    if (argc < 2)
    {
      std::cerr << "Usage: chat_server <port> [<port> ...]\n";
      return 1;
    }
    asio::io_context io_context;

    //making deck and shuffling
    d.build();
    d.shuffle();

    std::list<chat_server> servers; 

    // starting a server calls the do_accept() function
    for (int i = 1; i < argc; ++i) 
    { 
      tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[i]));
      servers.emplace_back(io_context, endpoint);
    }

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}