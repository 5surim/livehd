#include <iostream>
#include <list>
#include <random>
#include <set>
#include <sstream>

#include "fmt/core.h"
#include "hier_tree.hpp"
#include "i_resolve_header.hpp"

void Hier_tree::floorplan_dag_set(const std::list<Dag::pdag>& set, std::stringstream& outstr) {
  std::stringstream instr;
  unsigned int      node_counter = 0;

  I(set.size() > 0);

  instr << fmt::format("{}\n", set.size());

  for (auto pd : set) {
    I(pd->width != 0);
    I(pd->height != 0);
    // account for the fact that there may be more than one children of a given type
    node_counter++;
    // keep a few digits of precision to ourselves
    instr << fmt::format("{:.12f} {:.12f}\n", pd->width, pd->height);
  }

  if (floor_verbose) {
    fmt::print("\ninput string stream:\n{}", instr.str());
  }

  invoke_blobb(instr, outstr, node_counter > 8);
}

// read blobb output from a string and assign it to a floorplan
void Hier_tree::parse_blobb(std::stringstream& blobb_str) {
  floorplans.emplace_back();
  auto& fp = floorplans[floorplans.size() - 1];

  double tw, th;
  blobb_str >> tw;
  blobb_str >> th;

  fp.total_width  = tw;
  fp.total_height = th;

  size_t num_subs;
  blobb_str >> num_subs;

  for (size_t i = 0; i < num_subs; i++) {
    double w, h;

    blobb_str >> w;
    blobb_str >> h;

    fp.sub_fps.emplace_back();
    fp.sub_fps[i].width  = w;
    fp.sub_fps[i].height = h;
  }

  for (size_t i = 0; i < num_subs; i++) {
    double x, y;

    blobb_str >> x;
    blobb_str >> y;

    fp.sub_fps[i].xpos  = x;
    fp.sub_fps[i].ypos = y;
  }
}

// map an abstract floorplan back to a floorplan of actual nodes so we get connectivity information
void Hier_tree::map_floorplan() {}

void Hier_tree::dump_floorplans() {
  for (size_t i = 0; i < floorplans.size(); i++) {
    fmt::print("floorplan {}, width {:.3f}, height {:.3f}.\n", i, floorplans[i].total_width, floorplans[i].total_height);
    for (size_t j = 0; j < floorplans[i].sub_fps.size(); j++) {
      auto elem = floorplans[i].sub_fps[j];
      fmt::print("  element {} at ({}, {}) has width {:.3f}, height {:.3f}\n", j, elem.xpos, elem.ypos, elem.width, elem.height);
    }
  }
}

// recursively descent the hierarchy tree and send full floorplans to blobb.
// TODO: this is really slow, and not what HiReg asks for (I thought it was)
void Hier_tree::construct_recursive_floorplans() {
  for (size_t i = 0; i < hiers.size(); i++) {
    auto& dag         = dags[i];
    // auto& pattern_set = pattern_sets[i];

    // TODO: replace this with an unordered set
    std::list<Dag::pdag> subpatterns;

    auto push_children = [&](Dag::pdag pd) {
      for (size_t ci = 0; ci < pd->children.size(); ci++) {
        for (size_t count = 0; count < pd->child_edge_count[ci]; count++) {
          subpatterns.push_back(pd->children[ci]);
        }
      }
    };

    I(dag.root != nullptr);

    push_children(dag.root);

    std::stringstream outstr;

    bool has_patterns;
    do {
      // we're floorplanning a lot of nodes, so switching to hierarchical seems like a good idea
      // TODO: maybe also enumeration mode in BloBB? we aren't really going after optimal packings...
      floorplan_dag_set(subpatterns, outstr);
      parse_blobb(outstr);

      has_patterns = false;

      for (auto it = subpatterns.begin(); it != subpatterns.end(); it++) {
        auto pd = *it;
        if (!pd->is_leaf()) {
          subpatterns.erase(it);

          push_children(pd);
          has_patterns = true;
          break;
        }
      }
    } while (has_patterns);

    // floorplan all the leaf nodes too
    floorplan_dag_set(subpatterns, outstr);
    parse_blobb(outstr);
  }
}

void Hier_tree::construct_floorplans() {
  std::vector<pattern_id> valid_pats;

  std::unordered_map<Pattern, floorplan> fpm;

  // just floorplan a test pattern for now
  const size_t valid_ps   = 0;
  const size_t valid_p    = 0;
  auto         valid_pdag = dags[0].pat_dag_map[pattern_sets[valid_ps][valid_p]];

  double vw, vh;
  vw = valid_pdag->width;
  vh = valid_pdag->height;

  // find all floorplans that can fit inside valid_pats
  size_t valid_matches = 0;
  for (size_t ps = 0; ps < pattern_sets.size(); ps++) {
    for (size_t p = 0; p < pattern_sets[ps].size(); p++) {
      auto& pat = pattern_sets[ps][p];
      auto& pd  = dags[ps].pat_dag_map[pat];
      if (pd->width == vw && pd->height == vh) {
        valid_pats.push_back({ps, p});
        valid_matches++;
      }
    }
  }

  fmt::print("{} matches found.\n", valid_matches);

  I(false, "not implemented yet!");
}
