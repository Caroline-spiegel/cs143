#ifndef SEMANT_H_
#define SEMANT_H_

#include <assert.h>
#include <iostream>  
#include "cool-tree.h"
#include "stringtab.h"
#include "symtab.h"
#include "list.h"
#include <map>
#include <list>
#include <vector>
#include <set>
#include <utility>

#define TRUE 1
#define FALSE 0

class ClassTable;
typedef ClassTable *ClassTableP;

// This is a structure that may be used to contain the semantic
// information such as the inheritance graph.  You may use it or not as
// you like: it is only here to provide a container for the supplied
// methods.

class ClassTable {
private:

  /* PRIVATE MEMBER VARIABLES */
  Classes _classes;
  std::set<Symbol> _valid_classes;
  std::map<Symbol,int> _symbol_to_class_index_map;
  std::map<Symbol,Symbol> _child_to_parent_classmap;
  std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > _method_map;
  std::map<Symbol, Class_> _declared_classes_map;
  int semant_errors;
  
  void install_basic_classes();
  bool check_inheritance_graph_for_cycles();
  void gather_valid_classes();
  void verify_parent_classes_are_defined();
  void add_class_methods_to_method_map(Class__class *curr_class);
  void populate_child_parent_and_unique_ID_maps();

  ostream& error_stream;
public:

  ClassTable(Classes);
  std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > get_method_map();
  std::map<Symbol,Symbol> get_child_map(){ return _child_to_parent_classmap; }
  std::map<Symbol,Class_> get_class_map(){ return _declared_classes_map; }
  std::set<Symbol> get_class_set(){ return _valid_classes; }
  int errors() { return semant_errors; }
  ostream& semant_error();
  ostream& semant_error(Class_ c);
  ostream& semant_error(Symbol filename, tree_node *t);
  ostream& get_error_stream(){ return error_stream;}
  Classes get_class_list(){return _classes;}
  bool is_subtypeof(  Symbol child, Symbol supposed_parent, 
                                        std::map<Symbol,Symbol> _child_to_parent_classmap );
};


#endif

