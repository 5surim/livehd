//  This file is distributed under the BSD 3-Clause License. See LICENSE for details.

#include "pass_mockturtle.hpp"

void setup_pass_mockturtle() {
  Pass_mockturtle p;
  p.setup();
}

void Pass_mockturtle::setup() {
  Eprp_method m1("pass.mockturtle", "pass a lgraph using mockturtle", &Pass_mockturtle::work);

  register_pass(m1);
}

Pass_mockturtle::Pass_mockturtle()
    : Pass("mockturtle") {
}

void Pass_mockturtle::work(Eprp_var &var) {
  Pass_mockturtle pass;

  for(const auto &g : var.lgs) {
    pass.do_work(g);
  }
}

void Pass_mockturtle::lg_partition(LGraph *g) {
  std::map<int, int> group_id_mapping;
  int group_id = 0;

  for(const auto &nid : g->forward()) {
    auto node = Node(g,0,Node::Compact(nid)); // NOTE: To remove once new iterators are finished

    fmt::print("Node identifier:{}\n", node.get_compact());
    if (eligable_cell_op(node.get_type().op)) {

      if (group_boundary.find(node.get_compact())==group_boundary.end()) {

        int current_node_group_id = 0;

        for(const auto &in_edge : node.inp_edges()) {
          auto peer_driver_node = in_edge.driver.get_node();
          if (group_boundary.find(peer_driver_node.get_compact())!=group_boundary.end()) {
            current_node_group_id = group_boundary[peer_driver_node.get_compact()];
            break;
          }
        }

        if (current_node_group_id == 0) {
          group_id++;
          current_node_group_id = group_id;
        }
        group_boundary[node.get_compact()] = current_node_group_id;
      }

      for(const auto &out_edge : node.out_edges()) {
        auto peer_sink_node = out_edge.sink.get_node();
        if (eligable_cell_op(peer_sink_node.get_type().op)) {
          if (group_boundary.find(peer_sink_node.get_compact())==group_boundary.end()) {
            group_boundary[peer_sink_node.get_compact()] = group_boundary[node.get_compact()];
          }
          else {
            group_id_mapping[group_boundary[node.get_compact()]]=group_boundary[peer_sink_node.get_compact()];
          }
        }
      }
    }
  }

  for (std::map<int,int>::reverse_iterator rit = group_id_mapping.rbegin(); rit!=group_id_mapping.rend(); rit++) {
    for (auto &it:group_boundary) {
      if (it.second==rit->first)
        it.second=rit->second;
    }
  }

  for (const auto &it:group_boundary) {
    group_boundary_set[it.second].emplace_back(it.first);
  }

}

void Pass_mockturtle::create_LUT_network(LGraph *g) {
  for (const auto gid:group_boundary_set) {
    auto klut = mockturtle::klut_network();
    //gid2klut[gid.first]=klut;
    //traverse the nodes in lgraph and create nodes/signals in klut network
    for (const auto gid_node:gid.second) {
      auto cur_node = Node(g, 0, gid_node);
      switch (cur_node.get_type().op) {
        case Not_Op: {
          //Note: Don't need to check the node_pin pid since Not_Op has only one sink pin and one driver pin
          fmt::print("Not_Op in gid:{}\n",gid.first);
          //assuming there is only one input
          I(cur_node.inp_edges().size()==1);
          //must have output
          I(cur_node.out_edges().size()>0);

          std::vector<mockturtle::klut_network::signal> inp_sig, out_sig;
          unsigned long int loop_count = 0;
          for (const auto &in_edge : cur_node.inp_edges()) {
            fmt::print("input_bit_width:{}\n",in_edge.get_bits());
            //check if this input edge is already in the output mapping table
            //then setup the input signal accordingly
            if (edge2signal.count(in_edge)!=0) {
              for (auto i=0;i<in_edge.get_bits();i++) {
                inp_sig.emplace_back(edge2signal[in_edge][i]);
              }
            }
            else {
              for (auto i=0;i<in_edge.get_bits();i++) {
                inp_sig.emplace_back(klut.create_pi());
              }
              edge2signal[in_edge]=inp_sig;
            }
            //create the output signal
            for (auto i=0;i<in_edge.get_bits();i++) {
              out_sig.emplace_back(klut.create_not(inp_sig[i]));
            }
            loop_count++;
          }
          //this loop should be executed only once
          //since a Not_Op should only have single input edge
          I(loop_count==1);
          //mapping the output edge to output signal
          for (const auto &out_edge : cur_node.out_edges()) {
            fmt::print("output_bit_width:{}\n",out_edge.get_bits());
            //make sure the bit-width matches each other
            I(out_sig.size()==out_edge.get_bits());
            //check if the output edge is already in the input mapping table
            //then setup/update the output/input table
            if (edge2signal.count(out_edge)!=0) {
              //substitution the input signal by output signal
              for (auto i=0;i<out_edge.get_bits();i++) {
                klut.substitute_node(edge2signal[out_edge][i], out_sig[i]);
                //FIX ME: Remove dead node from klut network
                //K-Lut network doesn't support take_out_node()
                //To fix this problem, we should first create MIG network
                //then convert the whole MIG to K-Lut
              }
            }
            edge2signal[out_edge]=out_sig;
          }
          break;
        }

        case And_Op: {
          fmt::print("And_Op in gid:{}\n",gid.first);
          //And_Op must have more than one input
          I(cur_node.inp_edges().size()>1);
          //must have output
          I(cur_node.out_edges().size()>0);

          //mapping all input edge to input signal
          //create output signal at the same time
          std::vector<mockturtle::klut_network::signal> out_sig_0, out_sig_1;
          std::vector<std::vector<mockturtle::klut_network::signal>> inp_sig_group_by_bit;
          for (const auto &in_edge : cur_node.inp_edges()) {
            fmt::print("input_bit_width:{}\n",in_edge.get_bits());
            std::vector<mockturtle::klut_network::signal> inp_sig;
            //check if this input edge is already in the output mapping table
            //then setup the input signal accordingly
            if (edge2signal.count(in_edge)!=0) {
              for (long unsigned int i=0;i<in_edge.get_bits();i++) {
                if (inp_sig_group_by_bit.size()<=i) {
                  inp_sig_group_by_bit.resize(i+1);
                }
                inp_sig_group_by_bit[i].emplace_back(edge2signal[in_edge][i]);
              }
            } else {
              //input edge not mapped, create new signal and map it
              for (long unsigned int i=0;i<in_edge.get_bits();i++) {
                if (inp_sig_group_by_bit.size()<=i) {
                  inp_sig_group_by_bit.resize(i+1);
                }
                const auto x = klut.create_pi();
                inp_sig_group_by_bit[i].emplace_back(x);
                inp_sig.emplace_back(x);
              }
              //mapping the input edge to input signal
              edge2signal[in_edge]=inp_sig;
            }
          }
          //create the output signal
          for (long unsigned int i = 0; i < inp_sig_group_by_bit.size(); i++) {
            out_sig_0.emplace_back(klut.create_nary_and(inp_sig_group_by_bit[i]));
          }
          out_sig_1.emplace_back(klut.create_nary_and(out_sig_0));
          //mapping output edge to output signal
          for (const auto &out_edge : cur_node.out_edges()) {
            switch (out_edge.driver.get_pid()) {
              case 0: {
                I(out_edge.get_bits()==out_sig_0.size());
                if (edge2signal.count(out_edge)!=0) {
                  for (auto i=0;i<out_edge.get_bits();i++) {
                    klut.substitute_node(edge2signal[out_edge][i], out_sig_0[i]);
                    //FIX ME: removing dead signals/nodes.
                  }
                }
                edge2signal[out_edge] = out_sig_0;
                break;
              }
              case 1: {
                I(out_edge.get_bits()==out_sig_1.size());
                if (edge2signal.count(out_edge)!=0) {
                  for (auto i=0;i<out_edge.get_bits();i++) {
                    klut.substitute_node(edge2signal[out_edge][i], out_sig_1[i]);
                    //FIX ME: removing dead signals/nodes.
                  }
                }
                edge2signal[out_edge] = out_sig_1;
                break;
              }
              default:
                I(false);
                break;
            }
          }
          break;
        }
/*        case Or_Op:
          fmt::print("Or_Op in gid:{}\n",gid.first);
          break;
        case Xor_Op:
          fmt::print("Xor_Op in gid:{}\n",gid.first);
          break;*/
        default:
          fmt::print("Unknown_Op in gid:{}\n",gid.first);
          break;
      }

    }
    //output the unconnected nodes in the klut network
    fmt::print("KLUT network under Group ID:{}\n", gid.first);
    mockturtle::write_bench(klut,std::cout);

  }
}

void Pass_mockturtle::do_work(LGraph *g) {
  LGBench b("pass.mockturtle");

  //absl::flat_hash_map<Node::Compact, std::pair<std::string,std::pair<int,int>>> cell_type;

  int cell_amount = 0;
  for(const auto &nid : g->forward()) {
    auto node = Node(g,0,Node::Compact(nid)); // NOTE: To remove once new iterators are finished
    node.get_type(); //avoid warning
    cell_amount++;
/*
    auto name = node.get_type().get_name();
    int in_edges_num = 0;
    for(const auto &in_edge : node.inp_edges()) {
      in_edges_num++;
    }
    int out_edges_num = 0;
    for(const auto &out_edge : node.out_edges()) {
      out_edges_num++;
    }
    cell_type[node.get_compact()]=std::make_pair(name,std::make_pair(in_edges_num,out_edges_num));
*/
  }
//  fmt::print("Pass: number of cells {}\n", cell_type.size());
  fmt::print("{} nodes passed.\n", cell_amount);
/*
  for (auto const it:group_id_mapping) {
    fmt::print("GID:{}->{}\n", it.first, it.second);
  }
  for(auto const it:cell_type) {
    fmt::print("node_type:{} in_edges:{} out_edges:{}\n", it.second.first, it.second.second.first, it.second.second.second);
  }
*/
  fmt::print("Start partition...");

  lg_partition(g);

  for (const auto &group_id_it:group_boundary_set) {
    fmt::print("Group ID:{}\n", group_id_it.first);
    for (const auto &node_it:group_id_it.second) {
      fmt::print("Node identifier:{}\n", node_it);
    }
  }

  fmt::print("Creating k-LUT network...\n");

  create_LUT_network(g);


}
