

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "semant.h"
#include "utilities.h"
#include <map>
#include <list>
#include <vector>
#include <set>
#include <utility>
#include "cool-tree.h"
#include <iostream>

extern int semant_debug;
extern char *curr_filename;


//////////////////////////////////////////////////////////////////////
//
// Symbols
//
// For convenience, a large number of symbols are predefined here.
// These symbols include the primitive type and method names, as well
// as fixed names used by the runtime system.
//
//////////////////////////////////////////////////////////////////////
static Symbol 
    arg,
    arg2,
    Bool,
    concat,
    cool_abort,
    copy,
    Int,
    in_int,
    in_string,
    IO,
    length,
    Main,
    main_meth,
    No_class,
    No_type,
    Object,
    out_int,
    out_string,
    prim_slot,
    self,
    SELF_TYPE,
    Str,
    str_field,
    substr,
    type_name,
    val;
//
// Initializing the predefined symbols.
//
static void initialize_constants(void)
{
    arg         = idtable.add_string("arg");
    arg2        = idtable.add_string("arg2");
    Bool        = idtable.add_string("Bool");
    concat      = idtable.add_string("concat");
    cool_abort  = idtable.add_string("abort");
    copy        = idtable.add_string("copy");
    Int         = idtable.add_string("Int");
    in_int      = idtable.add_string("in_int");
    in_string   = idtable.add_string("in_string");
    IO          = idtable.add_string("IO");
    length      = idtable.add_string("length");
    Main        = idtable.add_string("Main");
    main_meth   = idtable.add_string("main");
    //   _no_class is a symbol that can't be the name of any 
    //   user-defined class.
    No_class    = idtable.add_string("_no_class");
    No_type     = idtable.add_string("_no_type");
    Object      = idtable.add_string("Object");
    out_int     = idtable.add_string("out_int");
    out_string  = idtable.add_string("out_string");
    prim_slot   = idtable.add_string("_prim_slot");
    self        = idtable.add_string("self");
    SELF_TYPE   = idtable.add_string("SELF_TYPE");
    Str         = idtable.add_string("String");
    str_field   = idtable.add_string("_str_field");
    substr      = idtable.add_string("substr");
    type_name   = idtable.add_string("type_name");
    val         = idtable.add_string("_val");
}



/* We perform some semantic analysis here.
  
   This function accepts "Classes", which is a typedef for typedef Classes_class *Classes
   "Classes_class" is in turn defined as "typedef list_node<Class_> Classes_class"
   
   We use the list_node interface, which is an implementation of a list.

   "Class_" is a type def for :typedef class Class__class *Class_"

   _declared_classes_map is a map that maps
    _std::map Type -> Class_ that keeps track of all the declared classes, 
    so Bool, Int, etc will have an entry there.
*/
ClassTable::ClassTable(Classes classes) : 	_classes(classes), 
                                            _valid_classes(),
                                            _symbol_to_class_index_map(),
                                            _child_to_parent_classmap(),
                                            _method_map(),
                                            _declared_classes_map(),   
                                            semant_errors(0) , 
                                            error_stream(cerr)
{
	// walk through each of the classes in the class list of the program
	//list_node<Class__class *> class_deep_copy = classes->copy_list(); // make a deep copy. We might need to modify it as we go???	

    // method map is indexed by Class Symbol, Method Symbol
    //std::set<Symbol> valid_classes;
	// PASS 1 -- make a set with all of the class names (not including the parent each is inherited from)
	install_basic_classes();
    gather_valid_classes();

	// PASS 2 -- MAKE SURE THAT EACH CLASS THAT WAS INHERITED FROM WAS REAL
	//verify_parent_classes_are_defined(_classes, valid_classes);

	// PASS 3 over the program
	// keep global counter of how many classes we have seen so far, and this is the unique ID for each class
	//populate_child_parent_and_unique_ID_maps(_classes, valid_classes); 
	
	// unique_class_idx holds the total number of classes
	bool is_cyclic = check_inheritance_graph_for_cycles();
	if (is_cyclic)
	{
		error_stream << "THROW ERROR! Graph is cyclic";
        semant_error();
	}
	// RETURN SOME VALUE return is_cyclic;

}

/*
   In PASS 1, we make a set with all of the class names 
   (not including the parent each is inherited from).
   The size of the set is how many classes we have seen so far.
*/
void ClassTable::gather_valid_classes( ) 
{

    // in what cases would this not be equal to classes.len()?
    for(int i = _classes->first(); _classes->more(i); i = _classes->next(i))
    {
            Class__class *curr_class = _classes->nth(i);
            Symbol curr_class_name = curr_class->get_name();
            
            if( _valid_classes.find(curr_class_name) == _valid_classes.end() )
            {
                    // does not exist in the map yet
               
                    _valid_classes.insert( curr_class_name );
            } else {
                    error_stream << "THROW ERROR! CLASS DEFINED TWICE\n" ;
            }
    }

    verify_parent_classes_are_defined( );
    populate_child_parent_and_unique_ID_maps(); 

}

/*
        // PASS 2 MAKE SURE THAT EACH CLASS THAT WAS INHERITED FROM WAS REAL
    // decremenet the size of the set (total number of classes) if any of them were fake
*/
void ClassTable::verify_parent_classes_are_defined( )
{

	for(int i = _classes->first(); _classes->more(i); i = _classes->next(i))
	{
		Class__class *curr_class = _classes->nth(i);
		Symbol child_class_name = curr_class->get_name();
		Symbol parent_class_name = curr_class->get_parent();
      
		// If it has no parent, parent is type Object
		if( (_valid_classes.find(parent_class_name) == _valid_classes.end())  )
		{
			_valid_classes.erase(child_class_name);
			error_stream << "THROW ERROR! child inherits from an undefined class\n";
		}
	}
}


void ClassTable::populate_child_parent_and_unique_ID_maps( )
{
    int unique_class_idx = 0;
	for(int i = _classes->first(); _classes->more(i); i = _classes->next(i))
    {
        Class__class *curr_class = _classes->nth(i);
        Symbol child_class_name = curr_class->get_name();

        Symbol parent_class_name = curr_class->get_parent();
        if( _valid_classes.find(child_class_name) != _valid_classes.end() )
        {


	       _declared_classes_map.insert(std::make_pair(child_class_name, curr_class));
            // besides acyclicity, we can say this class is legit bc it persisted in the valid classes set
            // if this class does not inherit from anybody, do not record it having any parent
            _child_to_parent_classmap.insert(std::make_pair(child_class_name, parent_class_name ));
            _symbol_to_class_index_map.insert(std::make_pair(child_class_name,unique_class_idx));
            unique_class_idx++;

            // add the class to the symbol table
            // add the methods of this class to the methodtable
            
            

        }else{
           
        }
    }

    for(int l = _classes->first(); _classes->more(l); l = _classes->next(l))
    {
        Class__class *curr_class = _classes->nth(l);

        add_class_methods_to_method_map(curr_class);
    }


    _child_to_parent_classmap.insert(std::make_pair(IO, Object));

    _symbol_to_class_index_map.insert(std::make_pair(IO,unique_class_idx));
    unique_class_idx +=1;
    _child_to_parent_classmap.insert(std::make_pair(Str, Object));
    _symbol_to_class_index_map.insert(std::make_pair(Str,unique_class_idx));
    unique_class_idx +=1;
    _child_to_parent_classmap.insert(std::make_pair(Bool, Object));
    _symbol_to_class_index_map.insert(std::make_pair(Bool,unique_class_idx));
    unique_class_idx +=1;
    _child_to_parent_classmap.insert(std::make_pair(Int, Object));
    _symbol_to_class_index_map.insert(std::make_pair(Int,unique_class_idx));
    unique_class_idx +=1;
	_child_to_parent_classmap.insert(std::make_pair(Object, No_class));
    _symbol_to_class_index_map.insert(std::make_pair(Object,unique_class_idx));
    
    assert(unique_class_idx+1 == (int)_valid_classes.size());

    // assert size of set is the same size as the number of unique classes
}


/*
        Add the methods of this class to the methodtable
*/
void ClassTable::add_class_methods_to_method_map(Class__class *curr_class)
{
    Symbol curr_class_name = curr_class->get_name();
    list_node<Feature> *curr_features = curr_class->get_features();
    for(int j = curr_features->first(); curr_features->more(j); j = curr_features->next(j))
    {
        Feature_class *curr_feat = curr_features->nth(j);
        // every feature is either a method, or an attribute
        if (curr_feat->feat_is_method() ) // I am a method! ;
        {
            // key is (curr class, name of the method)
            std::pair<Symbol,Symbol> key = std::make_pair( curr_class_name, curr_feat->get_name() );

            // the value is the std::vector<Symbols>, all parameters and then return type
            std::vector<Symbol> params_and_rt = std::vector<Symbol>( curr_feat->get_params_and_rt() );

            // if its a method, then we need to add it to the method table
            _method_map.insert(std::make_pair( key, params_and_rt ));
        }
    }


// while (curr_class_name!=Object && curr_class->get_parent()!=NULL){
// Class__class *par_class = _declared_classes_map.find(curr_class->get_parent())->second;
// list_node<Feature> *par_features = par_class->get_features();
//     for(int k = par_features->first(); par_features->more(k); k = par_features->next(k))
//     {
//         Feature_class *curr_feat = par_features->nth(k);
//         // every feature is either a method, or an attribute
//         if (curr_feat->feat_is_method() ) // I am a method! ;
//         {
//             // key is (curr class, name of the method)
//             std::pair<Symbol,Symbol> key = std::make_pair( curr_class_name, curr_feat->get_name() );

//             // the value is the std::vector<Symbols>, all parameters and then return type
//             std::vector<Symbol> params_and_rt = std::vector<Symbol>( curr_feat->get_params_and_rt() );

//             // if its a method, then we need to add it to the method table
//             _method_map.insert(std::make_pair( key, params_and_rt ));
//         }
//     }
// }

}


std::map<std::pair<Symbol,Symbol>, std::vector<Symbol> >  ClassTable::get_method_map()
{
    return _method_map;
}


 void ClassTable::install_basic_classes( ) {

    // The tree package uses these globals to annotate the classes built below.
   // curr_lineno  = 0;
    Symbol filename = stringtable.add_string("<basic class>");
    
    // The following demonstrates how to create dummy parse trees to
    // refer to basic Cool classes.  There's no need for method
    // bodies -- these are already built into the runtime system.
    
    // IMPORTANT: The results of the following expressions are
    // stored in local variables.  You will want to do something
    // with those variables at the end of this method to make this
    // code meaningful.

    // 
    // The Object class has no parent class. Its methods are
    //        abort() : Object    aborts the program
    //        type_name() : Str   returns a string representation of class name
    //        copy() : SELF_TYPE  returns a copy of the object
    //
    // There is no need for method bodies in the basic classes---these
    // are already built in to the runtime system.

    Class_ Object_class =
	class_(Object, 
	       No_class,
	       append_Features(
			       append_Features(
					       single_Features(method(cool_abort, nil_Formals(), Object, no_expr())),
					       single_Features(method(type_name, nil_Formals(), Str, no_expr()))),
			       single_Features(method(copy, nil_Formals(), SELF_TYPE, no_expr()))),
	       filename);

    // 
    // The IO class inherits from Object. Its methods are
    //        out_string(Str) : SELF_TYPE       writes a string to the output
    //        out_int(Int) : SELF_TYPE            "    an int    "  "     "
    //        in_string() : Str                 reads a string from the input
    //        in_int() : Int                      "   an int     "  "     "
    //
    Class_ IO_class = 
	class_(IO, 
	       Object,
	       append_Features(
			       append_Features(
					       append_Features(
							       single_Features(method(out_string, single_Formals(formal(arg, Str)),
										      SELF_TYPE, no_expr())),
							       single_Features(method(out_int, single_Formals(formal(arg, Int)),
										      SELF_TYPE, no_expr()))),
					       single_Features(method(in_string, nil_Formals(), Str, no_expr()))),
			       single_Features(method(in_int, nil_Formals(), Int, no_expr()))),
	       filename);  

    //
    // The Int class has no methods and only a single attribute, the
    // "val" for the integer. 
    //
    Class_ Int_class =
	class_(Int, 
	       Object,
	       single_Features(attr(val, prim_slot, no_expr())),
	       filename);

    //
    // Bool also has only the "val" slot.
    //
    Class_ Bool_class =
	class_(Bool, Object, single_Features(attr(val, prim_slot, no_expr())),filename);

    //
    // The class Str has a number of slots and operations:
    //       val                                  the length of the string
    //       str_field                            the string itself
    //       length() : Int                       returns length of the string
    //       concat(arg: Str) : Str               performs string concatenation
    //       substr(arg: Int, arg2: Int): Str     substring selection
    //       
    Class_ Str_class =
	class_(Str, 
	       Object,
	       append_Features(
			       append_Features(
					       append_Features(
							       append_Features(
									       single_Features(attr(val, Int, no_expr())),
									       single_Features(attr(str_field, prim_slot, no_expr()))),
							       single_Features(method(length, nil_Formals(), Int, no_expr()))),
					       single_Features(method(concat, 
								      single_Formals(formal(arg, Str)),
								      Str, 
								      no_expr()))),
			       single_Features(method(substr, 
						      append_Formals(single_Formals(formal(arg, Int)), 
								     single_Formals(formal(arg2, Int))),
						      Str, 
						      no_expr()))),
	       filename);


_valid_classes.insert(Str);
_valid_classes.insert(Int);
_valid_classes.insert(Bool);
_valid_classes.insert(Object);
_valid_classes.insert(IO);
   
 _declared_classes_map.insert(std::make_pair(Int, Int_class));
  _declared_classes_map.insert(std::make_pair(Bool,Bool_class));
   _declared_classes_map.insert(std::make_pair(Str, Str_class));
    _declared_classes_map.insert(std::make_pair(Object, Object_class));
     _declared_classes_map.insert(std::make_pair(IO, IO_class));

     add_class_methods_to_method_map(Object_class);
     add_class_methods_to_method_map(IO_class);
     add_class_methods_to_method_map(Str_class);
     add_class_methods_to_method_map(Int_class);
     add_class_methods_to_method_map(Bool_class);
  // Classes append2 = append_Classes(single_Classes(IO_class) ,single_Classes(Object_class));
    
   //Classes append1 =  append_Classes(append_Classes( single_Classes(Bool_class) , single_Classes(Str_class)), append_Classes(_classes, single_Classes(Int_class)));
 
 // _classes = append_Classes( append1, append2); 


}

////////////////////////////////////////////////////////////////////
//
// semant_error is an overloaded function for reporting errors
// during semantic analysis.  There are three versions:
//
//    ostream& ClassTable::semant_error()                
//
//    ostream& ClassTable::semant_error(Class_ c)
//       print line number and filename for `c'
//
//    ostream& ClassTable::semant_error(Symbol filename, tree_node *t)  
//       print a line number and filename
//
///////////////////////////////////////////////////////////////////

ostream& ClassTable::semant_error(Class_ c)
{                                                             
    return semant_error(c->get_filename(),c);
}    

ostream& ClassTable::semant_error(Symbol filename, tree_node *t)
{
    error_stream << filename << ":" << t->get_line_number() << ": ";
    return semant_error();
}

ostream& ClassTable::semant_error()                  
{                                                 
    semant_errors++;                            
    return error_stream;
} 



/*   This is the entry point to the semantic checker.

     Your checker should do the following two things:

     1) Check that the program is semantically correct
     2) Decorate the abstract syntax tree with type information
        by setting the `type' field in each Expression node.
        (see `tree.h')

     You are free to first do 1), make sure you catch all semantic
     errors. Part 2) can be done in a second stage, when you want
     to build mycoolc.

    For each class, we add attributes of classes from which it inherited.
 */
void program_class::semant()
{
    initialize_constants();

    /* ClassTable constructor may do some semantic analysis */
    ClassTable *classtable = new ClassTable(classes);

    std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > method_map = classtable->get_method_map();
    SymbolTable<Symbol,Symbol> *id_to_type_symtab = new SymbolTable<Symbol,Symbol>();
    std::set<Symbol> valid_classes = classtable->get_class_set();
    std::map<Symbol,Symbol> child_to_parent_classmap = classtable->get_child_map();
    std::map<Symbol, Class_> declared_classes_map = classtable->get_class_map();

    // Add Object?, IO, Int, Str, and Bool to the child_to_parent_classmap
   

    bool Main_class_missing = ( valid_classes.find(Main) == valid_classes.end() );
    if (Main_class_missing)
    {

        std::cerr << "Main class missing";
        classtable->semant_error();
    }

    bool main_method_missing = ( method_map.find(std::make_pair(Main, main_meth)) == method_map.end() );
    // check that main method takes no arguments
    if (main_method_missing)
    {
        std::cerr << "main method missing in Main class.";
        classtable->semant_error();
    }
    


    // Perform all type checking
    for(std::set<Symbol>::iterator it = valid_classes.begin(); it != valid_classes.end(); it++)
    {


        Symbol curr_class_symbol = *it; 
        if (!(curr_class_symbol== Bool || curr_class_symbol == Str || curr_class_symbol == IO || curr_class_symbol == Object || *it == Int)){
        Class__class *curr_class = declared_classes_map.find(curr_class_symbol)->second;
         
        verify_type_of_all_class_features(  id_to_type_symtab, 
                                            method_map,
                                            classtable, /* ostream& error_stream */
                                            curr_class_symbol, 
                                            child_to_parent_classmap,
                                            declared_classes_map);
        }
         
    }

    if (classtable->errors()) {
        cerr << "Compilation halted due to static semantic errors." << endl;
        exit(1);
    }
    // free the memory
    delete id_to_type_symtab;
    delete classtable; // automatically frees the method table
}

/*
    We perform type checking here. We descend down to the expressions of each class,
    and while looping through each one, type check it individually.
*/
void program_class::verify_type_of_all_class_features(  SymbolTable<Symbol,Symbol> *symtab,
                                                        std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > & method_map,
                                                        void* classtable, 
                                                        Symbol curr_class_symbol, 
                                                        std::map<Symbol,Symbol> & _child_to_parent_classmap,
                                                        std::map<Symbol,Class_> & declared_classes_map)
{

    list_node<Feature> *curr_features = (declared_classes_map.find(curr_class_symbol)->second)->get_features();
    for(int i = curr_features->first(); curr_features->more(i); i = curr_features->next(i))
    {

        symtab->enterscope();
std::set<Symbol> vlid_set = ((ClassTableP)(classtable))->get_class_set();
for(std::set<Symbol>::iterator iter = vlid_set.begin(); iter != vlid_set.end(); iter++)
    {
            Symbol curr_class = *iter;
            Symbol* address = new Symbol();
            Symbol curr_type = curr_class;
            *address = curr_type;
            symtab->addid(curr_class, address);
    }
  
    Symbol self_sym = SELF_TYPE;
    Symbol* addr = new Symbol();
    *addr  = self_sym;
    symtab->addid(self, addr);

add_own_attributes_to_scope(curr_class_symbol, declared_classes_map, symtab);
       
        add_parent_attributes_to_scope(_child_to_parent_classmap, 
                                        declared_classes_map, 
                                        curr_class_symbol,
                                        symtab);




        Feature_class *curr_feat = curr_features->nth(i);




        if (curr_feat->feat_is_method() )
        {

            //check number of formals
            //type check formals
            //type check return type
            
            //add formals

Formals formals_list = curr_feat->get_formals();
 std::vector<Symbol> true_formals = method_map.find(std::make_pair(curr_class_symbol, curr_feat->get_name()))->second; 
            

           
            
            for(int k = formals_list->first(); formals_list->more(k); k = formals_list->next(k)){
                
                Formal_class *forml = formals_list->nth(k);
                Symbol* type_decl = new Symbol();
                Symbol typ = *(forml->get_type_decl());
                *type_decl  = typ;
                Symbol name = forml->get_name();
                symtab->addid(name, type_decl);

            }
            Symbol method_type = (curr_feat->get_expression_to_check())->type_check(symtab, method_map, classtable, curr_class_symbol, _child_to_parent_classmap);
                //self type
            
            //if(method_type == SELF_TYPE ){

             //   method_type = curr_class_symbol;
           // }
            Symbol ret_type = curr_feat->get_return_type();
             
         //    if(ret_type == SELF_TYPE){
            
           //  ret_type = curr_class_symbol;
                
             //    if(((ClassTableP)classtable)->is_subtypeof(ret_type, method_type, _child_to_parent_classmap)){
              //   ret_type = method_type;

              // }
        // }
       if( !(( method_type==SELF_TYPE)&&(ret_type==SELF_TYPE) ) ){

            if(method_type == SELF_TYPE ){
                method_type = curr_class_symbol;
            }
            Symbol ret_type = curr_feat->get_return_type();

            if(ret_type == SELF_TYPE){
                ret_type = curr_class_symbol;
                if(((ClassTableP)classtable)->is_subtypeof(ret_type, method_type, _child_to_parent_classmap)){
                    ret_type = method_type;
                }
            }



	 if ( !((ClassTableP)classtable)->is_subtypeof(method_type, ret_type, _child_to_parent_classmap) ){
        ((ClassTableP)classtable)->get_error_stream() << "Inferred return type of method does not conform to declared return type."<<endl;
        ((ClassTableP)classtable)->semant_error();
       }
	}
      
    
           
        } else {
            
            Symbol attr_type = curr_feat->get_expression_to_check()->type_check(symtab, method_map, classtable, curr_class_symbol, _child_to_parent_classmap);

        }


        symtab->exitscope();
    }


}


void program_class::add_own_attributes_to_scope(Symbol curr_class, 
                                                std::map<Symbol,Class_> & declared_classes_map,
                                                SymbolTable<Symbol,Symbol> *id_to_type_symtab)
{
   
    // loop over each feature
    // if that feature is an attribute, add it to the symbol table
    
    list_node<Feature> *curr_features = (declared_classes_map.find(curr_class)->second)->get_features();
    for(int i = curr_features->first(); curr_features->more(i); i = curr_features->next(i))
    {
        Feature_class *curr_feat = curr_features->nth(i);
        if (!curr_feat->feat_is_method() )
        {

            Symbol* type_decl = new Symbol();
            Symbol typ= *curr_feat->get_type_decl();
            *type_decl  = typ;

            

            id_to_type_symtab->addid( curr_feat->get_name(), type_decl );
        } 
    }
}

void program_class::add_parent_attributes_to_scope(std::map<Symbol,Symbol> & child_to_parent_classmap, 
                                    std::map<Symbol,Class_> & declared_classes_map,
                                    Symbol curr_class,
                                    SymbolTable<Symbol,Symbol> *id_to_type_symtab)
{
   
    // get the first parent
    if(curr_class!=Object){
    Symbol parent = (child_to_parent_classmap.find(curr_class))->second;
    while( true ){
        // get the parent's features
        if(declared_classes_map.find(parent)==declared_classes_map.end()) break;
        list_node<Feature> *curr_features = (declared_classes_map.find(parent)->second)->get_features();
        for(int i = curr_features->first(); curr_features->more(i); i = curr_features->next(i))
        {
            Feature_class *curr_feat = curr_features->nth(i);
            if (!curr_feat->feat_is_method() )
            {

                Symbol* type_decl = new Symbol();
            Symbol typ= *curr_feat->get_type_decl();
            *type_decl  = typ;
                id_to_type_symtab->addid( curr_feat->get_name(), type_decl );
            } 
        }
        parent = child_to_parent_classmap.find(parent)->second;
    }
}
}



 /* USE TO DETECT A CYCLE IN A GRAPH */
class ClassGraph
{
    int V;    // No. of vertices
    std::list<int> *adj;    // Pointer to an array containing adjacency lists
    bool isCyclicUtil(int v, bool visited[], bool *rs);  // used by isCyclic()
public:
    ClassGraph(int V);   // Constructor
    void addEdge(int v, int w);   // to add an edge to graph
    bool isCyclic();    // returns true if there is a cycle in this graph
};

ClassGraph::ClassGraph(int V)
{
    this->V = V;
    adj = new std::list<int>[V];
}



void ClassGraph::addEdge(int v, int w)
{
    adj[v].push_back(w); // Add w to v’s list.
}

// This function is a variation of DFSUytil() in https://www.geeksforgeeks.org/archives/18212
bool ClassGraph::isCyclicUtil(int v, bool visited[], bool *recStack)
{
    if(visited[v] == false)
    {
        // Mark the current node as visited and part of recursion stack
        visited[v] = true;
        recStack[v] = true;

        // Recur for all the vertices adjacent to this vertex
        std::list<int>::iterator i;
        for(i = adj[v].begin(); i != adj[v].end(); ++i)
        {
            if ( !visited[*i] && isCyclicUtil(*i, visited, recStack) )
                return true;
            else if (recStack[*i])
                return true;
        }

    }
    recStack[v] = false;  // remove the vertex from recursion stack
    return false;
}



// Returns true if the graph contains a cycle, else false.
// This function is a variation of DFS() in https://www.geeksforgeeks.org/archives/18212
bool ClassGraph::isCyclic()
{
    // Mark all the vertices as not visited and not part of recursion
    // stack
    bool *visited = new bool[V];
    bool *recStack = new bool[V];
    for(int i = 0; i < V; i++)
    {
        visited[i] = false;
        recStack[i] = false;
    }

    // Call the recursive helper function to detect cycle in different
    // DFS trees
    for(int i = 0; i < V; i++)
        if (isCyclicUtil(i, visited, recStack))
            return true;

    return false;
}




/*  Code taken from
    https://www.geeksforgeeks.org/detect-cycle-in-a-graph/
*/
bool ClassTable::check_inheritance_graph_for_cycles()
{
    int num_classes = _valid_classes.size();
    ClassGraph g(num_classes);
    for (std::map<Symbol, Symbol>::iterator it=_child_to_parent_classmap.begin(); it!=_child_to_parent_classmap.end(); ++it){
        Symbol parent_class_name = it->second;
        // // if this class does not inherit from anybody, do not record it having any parent
        if (parent_class_name != Object)
        {
            Symbol child_class_name = it->first;
            int parent_idx = _symbol_to_class_index_map.find( parent_class_name )->second;
            int child_idx = _symbol_to_class_index_map.find( child_class_name )->second;
            g.addEdge(parent_idx, child_idx);
        } 
    }
    if(g.isCyclic())
    {
        std::cout << "Graph contains cycle" << std::endl;
        return true;
    } else {
        return false;
    }
}







Symbol *method_class::get_type_decl()
{
    return &Object;
}


Symbol static_dispatch_class::type_check(   SymbolTable<Symbol,Symbol> *symtab,
                                            std::map<std::pair<Symbol,Symbol>,
                                            std::vector<Symbol> > & method_map,
                                            void* classtable, 
                                            Symbol class_symbol, 
                                            std::map<Symbol,Symbol> _child_to_parent_classmap)
{
     bool disp_was_self = false;
    //must conform to the type as type_name
    Symbol dispatch_class = expr->type_check(symtab, method_map, classtable, class_symbol, _child_to_parent_classmap); 
   
    if(dispatch_class == SELF_TYPE){
        disp_was_self = true;
        dispatch_class = class_symbol;
   }

    if ( !is_subtypeof(dispatch_class, type_name, _child_to_parent_classmap) ){
        ((ClassTableP)classtable)->get_error_stream() << "Static dispatch class did not conform."<<endl;
        ((ClassTableP)classtable)->semant_error();
    }
    std::vector<Symbol> method_formals = method_map.find(std::make_pair(type_name, name))->second; 
    std::vector<Symbol> dispatch_formals; 
    for(int i = actual->first(); actual->more(i); i = actual->next(i))
    {
        dispatch_formals.push_back(actual->nth(i)->type_check(symtab, method_map, classtable, class_symbol, _child_to_parent_classmap));
    }
    // check number of args is right -- we added return type to method_formals
    if (dispatch_formals.size() != (method_formals.size()-1) )
    {
        ((ClassTableP)classtable)->get_error_stream() << "You tried to call a function, but didnt supply the right number of args."<<endl;
        ((ClassTableP)classtable)->semant_error();

    }
    for( size_t j = 0; j < dispatch_formals.size(); j++ )
    {
        //check that used dispatch formal is a subtype of declared method formal
        if ( !is_subtypeof(dispatch_formals[j], method_formals[j], _child_to_parent_classmap) ){
            ((ClassTableP)classtable)->get_error_stream() << "Dispatch formal did not conform."<<endl;
            ((ClassTableP)classtable)->semant_error();
        }
    }

    Symbol ret_type = method_formals[method_formals.size()-1];    // check if the return type is SELF_TYPE
   
    if ( ret_type == SELF_TYPE ){

      
        type = dispatch_class;
        if(disp_was_self){
            type = SELF_TYPE;
            return SELF_TYPE;
        }
        return dispatch_class; //dispatch_class; 
    } 
    type=ret_type;
    return ret_type;
}
/*
    Normal dispatch has args:
        (Expression expr, Symbol name, Expressions actual;)
*/
Symbol dispatch_class::type_check(  SymbolTable<Symbol,Symbol> *symtab,
                                    std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > & method_map,
                                    void* classtable, 
                                    Symbol class_symbol, 
                                    std::map<Symbol,Symbol> _child_to_parent_classmap)
{
    bool disp_was_self = false;
    //must conform to the type as type_name
    Symbol dispatch_class = expr->type_check(symtab, method_map, classtable, class_symbol, _child_to_parent_classmap); 
   if(dispatch_class == SELF_TYPE){
    disp_was_self = true;
    dispatch_class = class_symbol;
   }
    std::map<Symbol, Class_> declared_classes_map = ((ClassTableP)classtable)->get_class_map();
    std::vector<Symbol> method_formals;
    Symbol disp_class = dispatch_class;

    if(method_map.find(std::make_pair(dispatch_class, name)) == method_map.end()){

    while(true){

        if(_child_to_parent_classmap.find(disp_class) == _child_to_parent_classmap.end()){
            //no parents
        ((ClassTableP)classtable)->get_error_stream() << "No matching method declaration."<<endl;
        ((ClassTableP)classtable)->semant_error();
        return Object;

        }
        //get parent
        Symbol disp_parent = (_child_to_parent_classmap).find(disp_class)->second; 


        if(method_map.find(std::make_pair(disp_parent, name)) != method_map.end()){
            method_formals = method_map.find(std::make_pair(disp_parent, name))->second;
            break;
        }

        disp_class = disp_parent;
    }

}else{
        method_formals = method_map.find(std::make_pair(dispatch_class, name))->second;

    }


    std::vector<Symbol> dispatch_formals; 
    for(int i = actual->first(); actual->more(i); i = actual->next(i))
    {

        dispatch_formals.push_back(actual->nth(i)->type_check(symtab, method_map, classtable, class_symbol, _child_to_parent_classmap));
    }
    // check number of args is right -- we added return type to method_formals
    if (dispatch_formals.size() != (method_formals.size()-1) )
    {
        ((ClassTableP)classtable)->get_error_stream() << "You tried to call a function, but didnt supply the right number of args."<<endl;
        ((ClassTableP)classtable)->semant_error();
        return Object;

    }
    for( size_t j = 0; j < dispatch_formals.size(); j++ )
    {
        if(dispatch_formals[j] == SELF_TYPE){
            dispatch_formals[j]= class_symbol;
        }
        //check that used dispatch formal is a subtype of declared method formal
        if ( !is_subtypeof(dispatch_formals[j], method_formals[j], _child_to_parent_classmap) ){
            ((ClassTableP)classtable)->get_error_stream() << "Dispatch formal did not conform."<<endl;
            ((ClassTableP)classtable)->semant_error();
            return Object;
        }
    }
    Symbol ret_type = method_formals[method_formals.size()-1];
    // check if the return type is SELF_TYPE

    if ( ret_type == SELF_TYPE ){

      
        type = dispatch_class;
        if(disp_was_self){
            type = SELF_TYPE;
            return SELF_TYPE;
        }
        return dispatch_class; //dispatch_class; 
    } 
    type=ret_type;
    return ret_type;

} 


Symbol loop_class::type_check(     SymbolTable<Symbol,Symbol> *symtab,
                    std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > & method_map,
                    void* classtable, Symbol class_symbol, std::map<Symbol,Symbol> _child_to_parent_classmap)
{
  if ( pred->type_check(symtab, method_map, classtable, class_symbol, _child_to_parent_classmap) != Bool )
  {
     ((ClassTableP)classtable)->get_error_stream() << "You did not use a boolean predicate for the while loop"<<endl;
     ((ClassTableP)classtable)->semant_error();
  }

 body->type_check(symtab, method_map, classtable, class_symbol, _child_to_parent_classmap); 

  type = Object;
  return Object;
}



Symbol plus_class::type_check(     SymbolTable<Symbol,Symbol> *symtab,
                    std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > & method_map,
                    void* classtable, Symbol class_symbol, std::map<Symbol,Symbol> _child_to_parent_classmap)
{

  if(! (	(e1->type_check(symtab,method_map, classtable, class_symbol, _child_to_parent_classmap) == Int) 
	&& (e2 ->type_check(symtab,method_map, classtable, class_symbol, _child_to_parent_classmap) == Int))	 ){
  ((ClassTableP)classtable)->get_error_stream() << "Attempted to add two non-integers"<<endl;
   ((ClassTableP)classtable)->semant_error();
  }

  type = Int;
  return Int;
}


Symbol sub_class::type_check(SymbolTable<Symbol,Symbol> *symtab,
                    std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > & method_map,
                    void* classtable, Symbol class_symbol, std::map<Symbol,Symbol> _child_to_parent_classmap)
{

  if(! (    (e1->type_check(symtab,method_map,classtable,class_symbol, _child_to_parent_classmap) == Int)
            && (e2 ->type_check(symtab,method_map,classtable,class_symbol, _child_to_parent_classmap) == Int))     ){
     //error
    ((ClassTableP)classtable)->get_error_stream() << "Attempted to subtract two non-integers"<<endl;
    ((ClassTableP)classtable)->semant_error(); 
  }
  type = Int;

  return Int;

}



Symbol isvoid_class::type_check(SymbolTable<Symbol,Symbol> *symtab,
                    std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > & method_map,
                    void* classtable, Symbol class_symbol, std::map<Symbol,Symbol> _child_to_parent_classmap)
{
  e1->type_check(symtab, method_map, classtable, class_symbol, _child_to_parent_classmap); 
  type = Bool;
  return Bool;
}


Symbol no_expr_class::type_check(SymbolTable<Symbol,Symbol> *symtab,
                    std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > & method_map,
                    void* classtable, Symbol class_symbol, std::map<Symbol,Symbol> _child_to_parent_classmap)
{

    type = No_type;
  // can use "this" if needed
  return No_type;
}




Symbol mul_class::type_check(SymbolTable<Symbol,Symbol> *symtab,
                    std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > & method_map,
                    void* classtable, Symbol class_symbol, std::map<Symbol,Symbol> _child_to_parent_classmap)
{


  if(! (    (e1->type_check(symtab,method_map,classtable, class_symbol, _child_to_parent_classmap) == Int)
            && (e2 ->type_check(symtab,method_map,classtable, class_symbol, _child_to_parent_classmap) == Int))     ){
     //error
    ((ClassTableP)classtable)->get_error_stream() << "Attempted to multiply two non-integers"<<endl;
    ((ClassTableP)classtable)->semant_error();
  }

  type = Int;
  return Int;
}


 Symbol divide_class::type_check(SymbolTable<Symbol,Symbol> *symtab,
                        std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > & method_map,
                        void* classtable, Symbol class_symbol, std::map<Symbol,Symbol> _child_to_parent_classmap)
{

  if(! (    (e1->type_check(symtab,method_map,classtable, class_symbol, _child_to_parent_classmap) == Int)
            && (e2 ->type_check(symtab,method_map,classtable, class_symbol, _child_to_parent_classmap) == Int))     ){
     //error
    ((ClassTableP)classtable)->get_error_stream() << "Attempted to add two non-integers"<<endl;
    ((ClassTableP)classtable)->semant_error();

  }

  type = Int;
  return Int;
}


 Symbol neg_class::type_check(SymbolTable<Symbol,Symbol> *symtab,
                        std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > & method_map,
                       void* classtable, Symbol class_symbol, std::map<Symbol,Symbol> _child_to_parent_classmap)
{

  if(! (e1->type_check(symtab,method_map,classtable, class_symbol, _child_to_parent_classmap) == Int) ){
     //error
     ((ClassTableP)classtable)->get_error_stream() << "You tried to negate a non-integer"<<endl;
     ((ClassTableP)classtable)->semant_error();

  }

  type = Int;
  return Int;
}


Symbol lt_class::type_check(SymbolTable<Symbol,Symbol> *symtab,
                            std::map<std::pair<Symbol,Symbol>,
                            std::vector<Symbol> > & method_map,
                            void* classtable, 
                            Symbol class_symbol, 
                            std::map<Symbol,Symbol> _child_to_parent_classmap)
{
    if(! (((e1-> type_check(symtab,method_map,classtable, class_symbol, _child_to_parent_classmap)) == Int) && ((e2 -> type_check(symtab,method_map,classtable, class_symbol, _child_to_parent_classmap)) == Int))){
        ((ClassTableP)classtable)->get_error_stream() << "Attempted to compare two non-integers"<<endl;
        ((ClassTableP)classtable)->semant_error();
    }
    type = Bool;
    return Bool;
}



Symbol eq_class::type_check(SymbolTable<Symbol,Symbol> *symtab,
                            std::map<std::pair<Symbol,Symbol>,
                            std::vector<Symbol> > & method_map,
                            void* classtable, 
                            Symbol class_symbol, 
                            std::map<Symbol,Symbol> _child_to_parent_classmap)
{
    Symbol T1 = e1->type_check( symtab, method_map, classtable, class_symbol,_child_to_parent_classmap);
    Symbol T2 = e2->type_check( symtab, method_map,  classtable, class_symbol, _child_to_parent_classmap);

    if ( ((T1 == Bool) && (T2 != Bool)) || ((T2 == Bool) && (T1 != Bool)) ){
        ((ClassTableP)classtable)->get_error_stream() << "You tried to check different types for equality."<<endl;
        ((ClassTableP)classtable)->semant_error();
    }
    if ( ((T1 == Int) && (T2 != Int)) || ((T2 == Int) && (T1 != Int)) )
    {
       ((ClassTableP)classtable)->get_error_stream() << "You tried to check different types for equality v bad"<<endl;
        ((ClassTableP)classtable)->semant_error();
    }
    if ( ((T1 == Str) && (T2 != Str)) || ((T2 == Str) && (T1 != Str)) )
    {
        ((ClassTableP)classtable)->get_error_stream() << "You tried to check different types for equality v bad"<<endl;
        ((ClassTableP)classtable)->semant_error();
    }
    type = Bool;
    return Bool;
}



Symbol leq_class::type_check(    SymbolTable<Symbol,Symbol> *symtab,
                            std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > & method_map,
                            void* classtable, Symbol class_symbol, std::map<Symbol,Symbol> _child_to_parent_classmap)
{
    if(! (((e1-> type_check(symtab,method_map, classtable, class_symbol, _child_to_parent_classmap)) == Int) && ((e2 -> type_check(symtab,method_map,classtable, class_symbol, _child_to_parent_classmap)) == Int))){
        ((ClassTableP)classtable)->get_error_stream() << "Attempted to compare two non-integers"<<endl;
        ((ClassTableP)classtable)->semant_error();
    }
    type = Bool;
    return Bool;
}


Symbol comp_class::type_check(	SymbolTable<Symbol,Symbol> *symtab,
                            std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > & method_map,
                            void* classtable, Symbol class_symbol, std::map<Symbol,Symbol> _child_to_parent_classmap)
{
    if( ( e1->type_check(symtab, method_map, classtable, class_symbol, _child_to_parent_classmap) != Bool ) ) {
        ((ClassTableP)classtable)->get_error_stream() << "Attempted to get complement of a non Bool."<<endl;
        ((ClassTableP)classtable)->semant_error();
    }
    type = Bool;
    return Bool;
}


Symbol string_const_class::type_check(	SymbolTable<Symbol,Symbol> *symtab,
                                        std::map<std::pair<Symbol,Symbol>,
                                        std::vector<Symbol> > & method_map,
                                        void* classtable,
                                        Symbol class_symbol, 
                                        std::map<Symbol,Symbol> _child_to_parent_classmap)
{
    type = Str;
    return Str; 
}



/*
	When "new <type>" is defined, we return <type>
*/
Symbol new__class::type_check(  SymbolTable<Symbol,Symbol> *symtab,
                                std::map<std::pair<Symbol,Symbol>,
                                std::vector<Symbol> > & method_map,
                                void* classtable, 
                                Symbol class_symbol, 
                                std::map<Symbol,Symbol> _child_to_parent_classmap)

{
    type = type_name;
	return type_name;
}



Symbol object_class::type_check(    SymbolTable<Symbol,Symbol> *symtab,
                                    std::map<std::pair<Symbol,Symbol>,
                                    std::vector<Symbol> > & method_map,
                                    void* classtable, 
                                    Symbol class_symbol, 
                                    std::map<Symbol,Symbol> _child_to_parent_classmap)
{
    
    
    Symbol *type_of_ID = symtab->lookup(name);

    if( type_of_ID == NULL)
    {
        ((ClassTableP)classtable)->get_error_stream() << "This variable name was never defined"<<endl;
        ((ClassTableP)classtable)->semant_error();
        type = Object;
    } else {
        type = *type_of_ID;
    }

    
    return type;
}



Symbol bool_const_class::type_check(  SymbolTable<Symbol,Symbol> *symtab,
                                    std::map<std::pair<Symbol,Symbol>,
                                    std::vector<Symbol> > & method_map,
                                    void* classtable, 
                                    Symbol class_symbol, 
                                    std::map<Symbol,Symbol> _child_to_parent_classmap)
{
    type = Bool;
    return type;
}



Symbol int_const_class::type_check(   SymbolTable<Symbol,Symbol> *symtab,
                                    std::map<std::pair<Symbol,Symbol>,
                                    std::vector<Symbol> > & method_map,
                                    void* classtable,Symbol class_symbol, 
                                    std::map<Symbol,Symbol> _child_to_parent_classmap)
{
    type = Int;
    return type;
}


/*
	We enforce that 
		e1 is a subtype of T0
*/
Symbol let_class::type_check( SymbolTable<Symbol,Symbol> *symtab,
                            std::map<std::pair<Symbol,Symbol>,
                            std::vector<Symbol> > & method_map,
                            void* classtable, 
                            Symbol class_symbol, 
                            std::map<Symbol,Symbol> _child_to_parent_classmap)
{
    
    Symbol initType = init->type_check(symtab, method_map, classtable, class_symbol, _child_to_parent_classmap);
    if(initType== No_type){ initType = type_decl;}
    if(initType== SELF_TYPE){ initType = type_decl;}
    if( !is_subtypeof(initType, type_decl, _child_to_parent_classmap) )
    {
        ((ClassTableP)classtable)->get_error_stream() << "the let initialization was not a subtype of the declared type of the var"<<endl;
        ((ClassTableP)classtable)->semant_error();
    }

    symtab->enterscope(); 
    
    Symbol* address = new Symbol();
    Symbol curr_type = type_decl;
    *address = curr_type;
    symtab->addid(identifier, address); // add x temporarily to the symbol table
    Symbol bodyType = body->type_check(symtab, method_map, classtable, class_symbol, _child_to_parent_classmap);
    symtab->exitscope(); // x is removed from the symbol table
    type = bodyType; 
    return type;
}


/*
    Only field is "Expressions body"
*/
Symbol block_class::type_check( SymbolTable<Symbol,Symbol> *symtab,
                                std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > & method_map,
                                void* classtable, Symbol class_symbol, std::map<Symbol,Symbol> _child_to_parent_classmap)
{
    int num_exprs_in_block = body->len();
    for(int i = body->first(); body->more(i); i = body->next(i))
    {
        Symbol curr_expr_type = body->nth(i)->type_check(symtab, method_map, classtable, class_symbol, _child_to_parent_classmap);
        // type of a block is the value of the last expression
       
            type = curr_expr_type;

    }

    return type;
}



Symbol typcase_class::type_check( SymbolTable<Symbol,Symbol> *symtab,
                                std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > & method_map,
                                void* classtable, Symbol class_symbol, std::map<Symbol,Symbol> _child_to_parent_classmap)
{
        expr->type_check(symtab, method_map, classtable, class_symbol, _child_to_parent_classmap);
        std::list<Symbol> types;
        Symbol return_case; 
     for(int i = cases->first(); cases->more(i); i = cases->next(i)){
        //go through branches and add variables to scope
        symtab->enterscope();

    
    Symbol* address = new Symbol();
    Symbol curr_type = cases->nth(i)->get_type_decl();
    *address = curr_type;
    symtab->addid(cases->nth(i)->get_name(), address); // add x temporarily to the symbol table
    Symbol case_type = cases->nth(i)->get_expr()->type_check(symtab, method_map, classtable, class_symbol, _child_to_parent_classmap);
    types.push_back(case_type);
    symtab->exitscope(); // x is r
       
    }
    if(types.size() == 1){
        type = types.front();
        return types.front();
    }else{
      std::list<Symbol>::iterator it;
      return_case = least_upper_bound(types.front(),types.front(), class_symbol,  _child_to_parent_classmap );
    for (it = types.begin(); it != types.end(); ++it){
        return_case = least_upper_bound(return_case, *it, class_symbol, _child_to_parent_classmap );
    }
}
    type = return_case;
    return return_case; // FIX THIS, THIS IS WRONG
}



Symbol cond_class::type_check(SymbolTable<Symbol,Symbol> *symtab,
                            std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > & method_map,
                            void* classtable, Symbol class_symbol, std::map<Symbol,Symbol> _child_to_parent_classmap)
{

    if ( !(pred->type_check(symtab, method_map, classtable, class_symbol, _child_to_parent_classmap) == Bool) )
    {
        ((ClassTableP)classtable)->get_error_stream() << "You use a conditional (if/then/else) without a boolean predicate"<<endl;
        ((ClassTableP)classtable)->semant_error();
    }
    Symbol e1_type = then_exp->type_check(symtab, method_map, classtable, class_symbol, _child_to_parent_classmap);
    Symbol e2_type = else_exp->type_check(symtab, method_map, classtable, class_symbol, _child_to_parent_classmap);
    type = least_upper_bound (e1_type, e2_type, class_symbol, _child_to_parent_classmap);
    return type;
}


Symbol assign_class::type_check(  SymbolTable<Symbol,Symbol> *symtab,
                                std::map<std::pair<Symbol,Symbol>,std::vector<Symbol> > & method_map,
                                void* classtable, 
                                Symbol class_symbol, 
                                std::map<Symbol,Symbol> _child_to_parent_classmap)
{
    Symbol *enforced_type_of_ID = symtab->lookup(name); // check if "Id" is defined 
    if( enforced_type_of_ID == NULL)
    {
        ((ClassTableP)classtable)->get_error_stream() << "ID missing in symtab: You cannot assign a variable that was not declared as a class attribute"<<endl;
        ((ClassTableP)classtable)->semant_error();
    }
    Symbol found_expr_type = expr->type_check(symtab, method_map, classtable, class_symbol, _child_to_parent_classmap);

    if ( !is_subtypeof(found_expr_type, *enforced_type_of_ID, _child_to_parent_classmap) ){
        ((ClassTableP)classtable)->get_error_stream() << "Assign class did not conform."<<endl;
        ((ClassTableP)classtable)->semant_error();
    }
    type = found_expr_type; 
    return type;
}





Symbol Expression_class::least_upper_bound (Symbol symbol1, 
                                            Symbol symbol2, 
                                            Symbol class_symbol, 
                                            std::map<Symbol,Symbol> _child_to_parent_classmap)
{
    
    if(symbol1 == symbol2) return symbol1;
    if(symbol1 == SELF_TYPE && symbol2 == SELF_TYPE) return SELF_TYPE;
    if(symbol1 == SELF_TYPE && symbol2 != SELF_TYPE) symbol1 = class_symbol;
    if(symbol2 == SELF_TYPE && symbol1 != SELF_TYPE) symbol2 = class_symbol;

    std::list<Symbol> parents1; 
    parents1.push_back(symbol1);
     if(_child_to_parent_classmap.find(symbol1)!=_child_to_parent_classmap.end()){
    Symbol parent1 = _child_to_parent_classmap.find(symbol1)->second;

    while(true){

        parents1.push_back(parent1);
        if(_child_to_parent_classmap.find(parent1) == _child_to_parent_classmap.end()){
            break;
        }
        parent1 = (_child_to_parent_classmap).find(parent1)->second;  
    }
    
    }

    if(_child_to_parent_classmap.find(symbol2)!=_child_to_parent_classmap.end()){
    Symbol parent2 = symbol2;

    while(true){
        std::list<Symbol>::iterator it;
        for (it = parents1.begin(); it != parents1.end(); ++it){
            if(parent2 == *it)
            {
                return parent2;
            }
        }
        if(_child_to_parent_classmap.find(parent2) == _child_to_parent_classmap.end()){
            break;
        }
        parent2 = _child_to_parent_classmap.find(parent2)->second;
    }
}
    return Object; 
}


bool Expression_class::is_subtypeof(  Symbol child, Symbol supposed_parent, 
                                        std::map<Symbol,Symbol> _child_to_parent_classmap )
{
     if (child == supposed_parent) return true;
    if (supposed_parent == Object ) return true;

    // if the child is Object, the only way to get subtype_of is if the 
    // parent is also an object
    if ( (child==Object) && (supposed_parent != Object)) return false;
    
    std::list<Symbol> real_parents; 
    if(_child_to_parent_classmap.find(child)!=_child_to_parent_classmap.end()){
    Symbol real_parent = _child_to_parent_classmap.find(child)->second;
    while(true){
        real_parents.push_back(real_parent);
        if((_child_to_parent_classmap).find(real_parent)== _child_to_parent_classmap.end()){
            break;
        }
        real_parent = (_child_to_parent_classmap).find(real_parent)->second;  
    }
    }
    std::list<Symbol>::iterator it;
    for (it = real_parents.begin(); it != real_parents.end(); ++it){
        Symbol found_parent = *it; 
        if ( found_parent == supposed_parent) return true;
    }
    return false;
}

bool ClassTable::is_subtypeof(  Symbol child, Symbol supposed_parent, 
                                        std::map<Symbol,Symbol> _child_to_parent_classmap )
{
    if (child == supposed_parent) return true;
    if (supposed_parent == Object ) return true;

    // if the child is Object, the only way to get subtype_of is if the 
    // parent is also an object
    if ( (child==Object) && (supposed_parent != Object)) return false;
    
    std::list<Symbol> real_parents; 
    if(_child_to_parent_classmap.find(child)!=_child_to_parent_classmap.end()){
    Symbol real_parent = _child_to_parent_classmap.find(child)->second;
    while(true){
        real_parents.push_back(real_parent);
        if((_child_to_parent_classmap).find(real_parent)== _child_to_parent_classmap.end()){
            break;
        }
        real_parent = (_child_to_parent_classmap).find(real_parent)->second;  
    }
    }
    std::list<Symbol>::iterator it;
    for (it = real_parents.begin(); it != real_parents.end(); ++it){
        Symbol found_parent = *it; 
        if ( found_parent == supposed_parent) return true;
    }
    return false;
}
