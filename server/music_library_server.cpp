/**
 * @file
 *
 * This is the main server process.  When it starts it listens for clients.  It then
 * accepts remote commands for modifying/viewing the music database.
 *
 */

#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include <mutex>

#include "MusicLibrary.h"
#include "JsonMusicLibraryApi.h"

#include <cpen333/process/socket.h>

/**
 * Main thread function for handling communication with a single remote
 * client.
 *
 * @param lib shared library
 * @param api communication interface layer
 * @param id client id for printing messages to the console
 */
void service(MusicLibrary &lib, MusicLibraryApi &&api, int id) {
  //=========================================================
  // TODO: Implement thread safety
  //=========================================================
  static std::mutex serviceMutex;

  std::cout << "Client " << id << " connected" << std::endl;

  // receive message
  std::unique_ptr<Message> msg = api.recvMessage(); //msg is becoming a nullptr

  // continue while we don't have an error
  while (msg != nullptr) {

    // react and respond to message
    MessageType type = msg->type();
    switch (type) {
      case MessageType::ADD: {
        // process "add" message
        // get reference to ADD
        AddMessage &add = (AddMessage &) (*msg);
		{
			std::lock_guard<std::mutex> lock(serviceMutex);
			std::cout << "Client " << id << " adding song: " << add.song << std::endl;

			// add song to library
			bool success = false;

			success = lib.add(add.song);

			// send response
			if (success) {
				std::cout << "in success"<< std::endl;
				api.sendMessage(AddResponseMessage(add, MESSAGE_STATUS_OK));
				std::cout << "after api.sendMessage()" << std::endl;
			}
			else {
				api.sendMessage(AddResponseMessage(add,
					MESSAGE_STATUS_ERROR,
					"Song already exists in database"));
			}
		}
        break;
      }
	  case MessageType::REMOVE: {
		  //====================================================
		  // TODO: Implement "remove" functionality
		  //====================================================
		  // process "remove" message
		  // get reference to REMOVE
		  RemoveMessage &remove = (RemoveMessage &)(*msg);
		  {
			  std::lock_guard<std::mutex> lock(serviceMutex);
			  std::cout << "Client " << id << " removing song: " << remove.song << std::endl;

			  // remove song to library
			  bool success = false;

			  success = lib.remove(remove.song);

			  // send response
			  if (success) {
				  api.sendMessage(RemoveResponseMessage(remove, MESSAGE_STATUS_OK));
			  }
			  else {
				  api.sendMessage(RemoveResponseMessage(remove,
					  MESSAGE_STATUS_ERROR,
					  "failed to remove song from database"));
			  }
		  }
        break;
      }
	  case MessageType::SEARCH: {
		  // process "search" message
		  // get reference to SEARCH
		  SearchMessage &search = (SearchMessage &)(*msg);
		  {
			  std::lock_guard<std::mutex> lock(serviceMutex);
			  std::cout << "Client " << id << " searching for: "
				  << search.artist_regex << " - " << search.title_regex << std::endl;

			  // search library
			  std::vector<Song> results;

			  results = lib.find(search.artist_regex, search.title_regex);


			  // send response
			  api.sendMessage(SearchResponseMessage(search, results, MESSAGE_STATUS_OK));
		  }
        break;
      }
      case MessageType::GOODBYE: {
        // process "goodbye" message
		std::lock_guard<std::mutex> lock(serviceMutex);
        std::cout << "Client " << id << " closing" << std::endl;
        return;
      }
      default: {
		std::lock_guard<std::mutex> lock(serviceMutex);
        std::cout << "Client " << id << " sent invalid message" << std::endl;
      }
    }

    // receive next message
    msg = api.recvMessage();
  }
}

/**
 * Load songs from a JSON file and add them to the music library
 * @param lib music library
 * @param filename file to load
 */
void load_songs(MusicLibrary &lib, const std::string& filename) {

  // parse from file stream
  std::ifstream fin(filename);
  if (fin.is_open()) {
    JSON j;
    fin >> j;
    std::vector<Song> songs = JsonConverter::parseSongs(j);
    lib.add(songs);
  } else {
    std::cerr << "Failed to open file: " << filename << std::endl;
  }

}

void handleClient(cpen333::process::socket client, MusicLibrary lib, int id) {
	// create API handler
	JsonMusicLibraryApi api(std::move(client));
	std::cout << "past making api" << std::endl;
	// service client-server communication
	service(lib, std::move(api), id);
	std::cout << "past making service" << std::endl;
	return;
}

int main() {

  // load  data
  std::vector<std::string> filenames = {
      "data/billboard_hot_100.json",
      "data/billboard_greatest_hot_100.json",
      "data/billboard_adult_contemporary.json",
      "data/billboard_adult_pop.json",
      "data/billboard_alternative.json",
      "data/billboard_country.json",
      "data/billboard_electronic.json",
      "data/billboard_jazz.json",
      "data/billboard_r&b.json",
      "data/billboard_rap.json",
      "data/billboard_rock.json",
  };

  MusicLibrary lib;       // main shared music library

  // load music library files
  for (const auto &filename : filenames) {
    load_songs(lib, filename);
  }

  // start server
  cpen333::process::socket_server server(MUSIC_LIBRARY_SERVER_PORT);
  server.open();
  std::cout << "Server started on port " << server.port() << std::endl;

  //===============================================================
  // TODO: Modify to allow multiple client-server connections
  //     Loop:
  //       - 'Accept' a socket client
  //       - Create an API wrapper around the socket
  //       - Send the API wrapper to the service(...) function
  //         to run in a new detached thread
  //===============================================================

  // create and store threads
  std::vector<std::thread> threads;
  int idNum = 0;
  cpen333::process::socket client;
  while (server.accept(client)) {

	  // create API handler
	  //JsonMusicLibraryApi api(std::move(client));
	  // service client-server communication
	  //service(lib, std::move(api), 0); 
	  std::thread clientThread(handleClient, std::move(client), std::ref(lib), ++idNum);
	  clientThread.detach();
	  //threads.push_back(std::thread(handleClient, std::move(client), std::ref(lib), ++idNum));
  }
  /*int numThreads = threads.size();
  for (int i = 0; i < numThreads; i++) {
  	  //threads[i].join();
	  threads[i].detach();
  }*/

  //or:
  /*
  while(server.accept(client)){
  std::thread(handleClient, std::move(client), std::ref(lib))
  thread.detach();
  }
  */
  //close server
  server.close();

  return 0;
}