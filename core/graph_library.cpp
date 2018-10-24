//  This file is distributed under the BSD 3-Clause License. See LICENSE for details.

#include <sys/types.h>
#include <dirent.h>

#include "graph_library.hpp"

std::unordered_map<std::string, Graph_library *> Graph_library::global_instances;

std::map<std::string, std::map<std::string, LGraph *>> Graph_library::global_name2lgraph;

LGraph *Graph_library::get_graph(uint32_t id) const {
  assert(attribute.size() > (size_t)id);

  const std::string &name = attribute[id].name;

  return find_lgraph(path, name);
}

LGraph *Graph_library::find_lgraph(const std::string &path, const std::string &name) {

  if(global_name2lgraph.find(path) != global_name2lgraph.end() && global_name2lgraph[path].find(name) != global_name2lgraph[path].end()) {
    LGraph *lg = global_name2lgraph[path][name];
    assert(global_instances.find(path) != global_instances.end());
    global_instances[path]->register_lgraph(name, lg);

    return lg;
  }

  return nullptr;
}

void Graph_library::reload() {

	assert(graph_library_clean);

  max_version = 0;
  std::ifstream graph_list;

  graph_list.open(path + "/" + library_file);

  name2id.clear();
  attribute.resize(1); // 0 is not a valid ID

  if(!graph_list.is_open()) {
    DIR *dir = opendir(path.c_str());
    if(dir) {
      struct dirent *dent;
      while((dent=readdir(dir))!=NULL) {
        if (dent->d_type != DT_REG) // Only regular files
          continue;
        if (strncmp(dent->d_name,"lgraph_", 7) != 0) // only if starts with lgraph_
          continue;
        int len = strlen(dent->d_name);
        if (strcmp(dent->d_name + len - 5,"_type") != 0) // and finish with _type
          continue;
        std::string name(dent->d_name + 7, len-5-7); 

        add_name(name);
      }
      closedir(dir);
    }
    return;
  }

  uint32_t n_graphs = 0;
  graph_list >> n_graphs;
  attribute.resize(n_graphs+1);

  for(size_t idx = 0; idx < n_graphs; ++idx) {
    std::string name;
    int         graph_version;
    size_t      graph_id;

    graph_list >> name >> graph_id >> graph_version;

    if (graph_version>=max_version)
      max_version = graph_version;

    name2id[name] = graph_id;

    // this is only true in case where we skip graph ids
    if(attribute.size() <= graph_id)
      attribute.resize(graph_id + 1);

    attribute[graph_id].name     = name;
    attribute[graph_id].version  = graph_version;
  }

  graph_list.close();
}

Graph_library::Graph_library(const std::string &_path)
  : path(_path)
  , library_file("graph_library") {

	graph_library_clean = true;
  reload();
}

void Graph_library::clean_library() {
  if(graph_library_clean)
    return;

  std::ofstream graph_list;

  graph_list.open(path + "/" + library_file);
  graph_list << attribute.size() << std::endl;
  uint32_t id=0;
  for(const auto &it : attribute) {
    graph_list << it.name << " " << id << " " << it.version << std::endl;
    id++;
  }

  graph_list.close();

  graph_library_clean = true;
}