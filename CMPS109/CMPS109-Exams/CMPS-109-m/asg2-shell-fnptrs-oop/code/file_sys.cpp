// Seongwoo Choi
// scho29
// 1368039

#include <iostream>
#include <stdexcept>
#include <unordered_map>
// included <iomanip> because this library allows
// you to have parametric manipulators
#include <iomanip>

using namespace std;

#include "debug.h"
#include "file_sys.h"

int inode::next_inode_nr {1};

struct file_type_hash {
   size_t operator() (file_type type) const {
      return static_cast<size_t> (type);
   }
};

// we do not need to modify it. 
ostream& operator<< (ostream& out, file_type type) {
   static unordered_map<file_type,string,file_type_hash> hash {
      {file_type::PLAIN_TYPE, "PLAIN_TYPE"},
      {file_type::DIRECTORY_TYPE, "DIRECTORY_TYPE"},
   };
   return out << hash[type];
}


inode_state::inode_state() {
   root = make_shared<inode>(file_type::DIRECTORY_TYPE);
   root->get_contents()->init_dir(root, root);
   cwd = root;

   DEBUGF ('i', "root = " << root << ", cwd = " << cwd
          << ", prompt = \"" << prompt() << "\"");
}

// Shared_ptr manages the stprage of a ppinter, providing a limited 
// garbage collection facility, possibly sharing that management
// with other objects. -cplusplus.com reference-
const string& inode_state::prompt() { return prompt_; }
inode_ptr inode_state::get_root() { return root; }
inode_ptr inode_state::get_cwd() { return cwd; }

string inode_state::pwd(inode_ptr curr) {
   string pwd = "";
   while(curr != root) {
      base_file_ptr dir = curr->get_contents();
      int inode_nr = dir->get_diren(".")->get_inode_nr();
      curr = dir->get_diren(".."); // Parent dir
      dir = curr->get_contents();
      pwd = "/" + dir->get_name(inode_nr) + pwd;
   }
   return pwd;
}

// ls tells what the current area of the terminal has. 
void inode_state::ls(inode_state& state, inode_ptr node, bool recur) {
   if(node == nullptr) return;
   cout << pwd(node) << ":" << endl;
   base_file_ptr dir = node->get_contents();
   dir->print_contents(state, recur);
}

inode_ptr inode_state::find(const string& dir,
                            const int& new_node) {
   if(dir == "/")
      return root;

   inode_ptr curr = cwd;
   wordvec file_path = split(dir, "/"); 
   string last = "";
   for(auto itor = file_path.begin();
         itor != file_path.end() - new_node; ++itor) {
      if(!curr->is_dir())
         throw file_error(*itor + " is not a directory");
      
      base_file_ptr dir = curr->get_contents();
      if(!dir->has_diren(*itor))
         throw file_error(*itor + " does not exist");
      curr = dir->get_diren(*itor);
      last = *itor;
   }
   
   if(new_node && !curr->is_dir())
      throw file_error(last + " is not a directory");
   
   return curr;
}

// this area finds parent.
inode_ptr inode_state::find_parent(const string& dir,
                                   const int& new_node) {
   if(dir == "/")
      return root;

   inode_ptr curr = cwd;
   wordvec file_path = split(dir, "/");
   inode_ptr parent = curr;
   string last = "";
   for(auto itor = file_path.begin();
         itor != file_path.end() - new_node; ++itor) {
      if(!curr->is_dir())
         throw file_error(*itor + " is not a directory");

      base_file_ptr dir = curr->get_contents();
      if(!dir->has_dirent(*itor))
         throw file_error(*itor + " does not exist");
      parent = curr;
      curr = dir->get_dirent(*itor);
      last = *itor;
   }
   
   if(new_node && !curr->is_dir())
      throw file_error(last + " is not a directory");
   return parent;
}

void inode_state::cd(const string& path) {
   if(path == "/") {
      cwd = root;
      return;
   }
   
   inode_ptr node = find(path, 0);

   if(!node->is_dir())
      throw file_error("cd: targetted a plain_file");
   
   cwd = node;
}

void inode_state::rm(inode_ptr par, const string& path, 
                     const bool& recur) {
   base_file_ptr dir = par->get_contents();
   inode_ptr curr = dir->get_diren(path);

   if(recur) {
      
   }
   if(curr != root) {
      if(curr->is_plain()) {
         base_file_ptr parent = par->get_contents();
         parent->remove(path);
      }
   }
}



void inode_state::set_prompt(const string& prompt) { 
   prompt_ = prompt;
}
void inode_state::set_root(inode_ptr newroot) { 
   root = newroot;
}
void inode_state::set_cwd(inode_ptr newcwd) { 
   cwd = newcwd;
}



ostream& operator<< (ostream& out, const inode_state& state) {
   out << "inode_state: root = " << state.root
       << ", cwd = " << state.cwd;
   return out;
}

/**
 * Inode
 */

inode::inode(file_type type): inode_nr (next_inode_nr++) {
   this->type = type;
   switch (type) {
      case file_type::PLAIN_TYPE:
           contents = make_shared<plain_file>();
           break;
      case file_type::DIRECTORY_TYPE:
           contents = make_shared<directory>();
           break;
   }
   DEBUGF ('i', "inode " << inode_nr << ", type = " << type);
}

int inode::get_inode_nr() const {
   DEBUGF ('i', "inode = " << inode_nr);
   return inode_nr;
}

base_file_ptr inode::get_contents() const {
   return contents;
}

bool inode::is_plain() {
   return type == file_type::PLAIN_TYPE;
}

bool inode::is_dir() { 
   return type == file_type::DIRECTORY_TYPE; 
}

/**
 * Files
 */


file_error::file_error (const string& what):
            runtime_error (what) {
}

size_t plain_file::size() const {
   size_t size {0};
   if(data.size() > 0) {
      for(auto const word: data) {
         // Add 1 for spaces
         size += word.length() + 1;
      }
      --size; // For last word's lack of space
   }
   DEBUGF ('i', "size = " << size);
   return size;
}

const wordvec& plain_file::readfile() const {
   DEBUGF ('i', data);
   return data;
}

void plain_file::writefile (const wordvec& words) {
   data.clear();
   for(auto itor = words.begin(); itor != words.end(); ++itor)
      data.push_back(*itor);
   DEBUGF ('i', words);
}

// maybe include rm function here. 
void plain_file::remove (const string&) {
   throw file_error ("is a plain file");
}

inode_ptr plain_file::mkdir (const string&) {
   throw file_error ("is a plain file");
}

inode_ptr plain_file::mkfile (const string&) {
   throw file_error ("is a plain file");
}

bool plain_file::has_diren(const string&) const {
   throw file_error ("is a plain file");
}

inode_ptr plain_file::get_diren(const string&) const {
   throw file_error ("is a plain file");
}

string plain_file::get_name(const int&) const {
   throw file_error ("is a plain file");
}

void plain_file::print_contents(inode_state& state, const bool&) {
   throw file_error ("is a plain file");
   cout << state << endl; 
}

void plain_file::orin_dir(inode_ptr a, inode_ptr b) {
   throw file_error ("is a plain file");
   auto temp = a;
   temp = b;
}

/**
 * Directories
 */


size_t directory::size() const {
   size_t size {dirents.size()};
   DEBUGF ('i', "size = " << size);
   return size;
}

const wordvec& directory::readfile() const {
   throw file_error ("is a directory");
}

void directory::writefile (const wordvec&) {
   throw file_error ("is a directory");
}

void directory::remove (const string& filename) {
   dirents.erase(filename);
   DEBUGF ('i', filename);
}

inode_ptr directory::mkdir (const string& dirname) {
   if(dirents.count(dirname) == 1)
      throw file_error ("mkdir: directory exists");
   
   inode_ptr dir = make_shared<inode>(file_type::DIRECTORY_TYPE);
   dir->get_contents()->orin_dir(dirents.at("."), dir);
   dirents.insert(pair<string, inode_ptr>(dirname, dir));
   
   DEBUGF ('i', dirname);
   return dir;
}

inode_ptr directory::mkfile (const string& filename) {
   if(has_diren(filename))
      return get_diren(filename);
   inode_ptr file = make_shared<inode>(file_type::PLAIN_TYPE);
   dirents.insert(pair<string, inode_ptr>(filename, file));

   DEBUGF ('i', filename);
   return file;
}

bool directory::has_diren(const string& key) const {
   return dirents.count(key) != 0;
}

inode_ptr directory::get_diren(const string& key) const {
   if(dirents.count(key) != 0)
      return dirents.at(key);
   throw file_error(key + ": does not exist");
}

string directory::get_name(const int& inode_nr) const {
   for(auto itor = dirents.begin(); 
         itor != dirents.end(); ++itor)
      if(itor->second->get_inode_nr() == inode_nr)
         return itor->first;
   return nullptr;
}

// this is where <iomanip> library is useful. It finds the field width.
void directory::print_contents(inode_state& state, const bool& recur) {
   for(auto itor = dirents.begin(); 
      itor != dirents.end(); ++itor) {
      
      string name = itor->first;
      inode_ptr node = itor->second;
      cout << setw(6) << node->get_inode_nr() << "  "
           << setw(6) << node->get_contents()->size()
           << "  " << name << ((node->is_dir() 
           && name != "." && name != "..")? "/": "")
           << endl;
   }
   if(recur) {
      for(auto itor = dirents.begin();
         itor != dirents.end(); ++itor) {
         if(itor->first != "." && itor->first != ".."
            && itor->second->is_dir()) {
            base_file_ptr dir = itor->second->get_contents();
            cout << state.pwd(itor->second) << ":" << endl;
            dir->print_contents(state, true);
         }
      }
   }
}

void directory::orin_dir(inode_ptr par, inode_ptr self) {
   dirents.insert(pair<string,inode_ptr>(".", self));
   dirents.insert(pair<string,inode_ptr>("..", par));
}

