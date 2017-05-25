// $Id: cixd.cpp,v 1.7 2016-05-09 16:01:56-07 - - $
// Seongwoo Choi
// scho29
// 1368039

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

#include <libgen.h>
#include <sys/types.h>
#include <unistd.h>

#include "protocol.h"
#include "logstream.h"
#include "sockets.h"

logstream log (cout);
struct cix_exit: public exception {};

void reply_ls (accepted_socket& client_sock, cix_header& header) {
   FILE* ls_pipe = popen ("ls -l", "r");
   if (ls_pipe == NULL) { 
      log << "ls -l: popen failed: " << strerror (errno) << endl;
      header.command = cix_command::NAK;
      header.nbytes = errno;
      send_packet (client_sock, &header, sizeof header);
   }
   string ls_output;
   char buffer[0x1000];
   for (;;) {
      char* rc = fgets (buffer, sizeof buffer, ls_pipe);
      if (rc == nullptr) break;
      ls_output.append (buffer);
   }
   header.command = cix_command::LSOUT;
   header.nbytes = ls_output.size();
   memset (header.filename, 0, FILENAME_SIZE);
   log << "sending header " << header << endl;
   send_packet (client_sock, &header, sizeof header);
   send_packet (client_sock, ls_output.c_str(), ls_output.size());
   log << "sent " << ls_output.size() << " bytes" << endl;
   pclose(ls_pipe);
}

void reply_get(accepted_socket& client_sock, cix_header& header) {
   ifstream of(header.filename);
   stringstream sstream;
   if (!of.good()) {
      log << "get: " << header.filename << ": no file" << endl;
      header.command = cix_command::NAK;
      header.nbytes = errno;
      send_packet(client_sock, &header, sizeof header);
      return;
   }
   sstream << of.rdbuf();
   string name = sstream.str();
   header.command = cix_command::FILE;
   header.nbytes = name.size();
   memset(header.filename, 0, FILENAME_SIZE);
   log << "sending header" << header << endl;
   send_packet(client_sock, &header, sizeof header);
   send_packet(client_sock, name.c_str(), name.size());
   log << "sent " << name.size() << " bytes" << endl;
}

void reply_put(accepted_socket& client_sock, cix_header& header) {
   ofstream of(header.filename);
   string out;
   if (!of.good()) {
      log << "put: cannot open file" << endl;
      header.command = cix_command::NAK;
      send_packet(client_sock, &header, sizeof header);
      return;
   }
   char buffer[header.nbytes+1];
   recv_packet(client_sock, buffer, header.nbytes);
   log << "received " << header.nbytes << "bytes" << endl;
   of.write(buffer, header.nbytes);
   of.close();
   header.command = cix_command::ACK;
   header.nbytes = 0;
   log << "sending header " << header << endl;
   send_packet(client_sock, &header, sizeof header);
}

void reply_rm(accepted_socket& client_sock, cix_header& header) {
   int val = unlink(header.filename);
   if (val != 0) {
      log << "rm: unlink failed " << strerror(errno) << endl;
      header.command = cix_command::NAK;
      header.nbytes = errno;
      send_packet(client_sock, &header, sizeof header);
      return;
   }
   header.command = cix_command::ACK;
   header.nbytes = 0;
   send_packet(client_sock, &header, sizeof header);
}

void run_server (accepted_socket& client_sock) {
   log.execname (log.execname() + "-server");
   log << "connected to " << to_string (client_sock) << endl;
   try {   
      for (;;) {
         cix_header header; 
         recv_packet (client_sock, &header, sizeof header);
         log << "received header " << header << endl;
         switch (header.command) {
            case cix_command::LS: 
               reply_ls (client_sock, header);
               break;
            case cix_command::GET:
               reply_get (client_sock, header);
               break;
            case cix_command::PUT:
               reply_put(client_sock, header);
               break;
            case cix_command::RM:
               reply_rm(client_sock, header);
               break;
            default:
               log << "invalid header from client" << endl;
               log << "cix_nbytes = " << header.nbytes << endl;
               log << "cix_command = " << header.command << endl;
               log << "cix_filename = " << header.filename << endl;
               break;
         }
      }
   }catch (socket_error& error) {
      log << error.what() << endl;
   }catch (cix_exit& error) {
      log << "caught cix_exit" << endl;
   }
   log << "finishing" << endl;
   throw cix_exit();
}

void fork_cixserver (server_socket& server, accepted_socket& accept) {
   pid_t pid = fork();
   if (pid == 0) { // child
      server.close();
      run_server (accept);
      throw cix_exit();
   }else {
      accept.close();
      if (pid < 0) {
         log << "fork failed: " << strerror (errno) << endl;
      }else {
         log << "forked cixserver pid " << pid << endl;
      }
   }
}


void reap_zombies() {
   for (;;) {
      int status;
      pid_t child = waitpid (-1, &status, WNOHANG);
      if (child <= 0) break;
      log << "child " << child
           << " exit " << (status >> 8)
           << " signal " << (status & 0x7F)
           << " core " << (status >> 7 & 1) << endl;
   }
}

void signal_handler (int signal) {
   log << "signal_handler: caught " << strsignal (signal) << endl;
   reap_zombies();
}

void signal_action (int signal, void (*handler) (int)) {
   struct sigaction action;
   action.sa_handler = handler;
   sigfillset (&action.sa_mask);
   action.sa_flags = 0;
   int rc = sigaction (signal, &action, nullptr);
   if (rc < 0) log << "sigaction " << strsignal (signal) << " failed: "
                   << strerror (errno) << endl;
}


int main (int argc, char** argv) {
   log.execname (basename (argv[0]));
   log << "starting" << endl;
   vector<string> args (&argv[1], &argv[argc]);
   signal_action (SIGCHLD, signal_handler);
   in_port_t port = get_cix_server_port (args, 0);
   try {
      server_socket listener (port);
      for (;;) {
         log << to_string (hostinfo()) << " accepting port "
             << to_string (port) << endl;
         accepted_socket client_sock;
         for (;;) {
            try {
               listener.accept (client_sock);
               break;
            }catch (socket_sys_error& error) {
               switch (error.sys_errno) {
                  case EINTR:
                     log << "listener.accept caught "
                         << strerror (EINTR) << endl;
                     break;
                  default:
                     throw;
               }
            }
         }
         log << "accepted " << to_string (client_sock) << endl;
         try {
            fork_cixserver (listener, client_sock);
            reap_zombies();
         }catch (socket_error& error) {
            log << error.what() << endl;
         }
      }
   }catch (socket_error& error) {
      log << error.what() << endl;
   }catch (cix_exit& error) {
      log << "caught cix_exit" << endl;
   }
   log << "finishing" << endl;
   return 0;
}