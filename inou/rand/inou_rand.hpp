//  This file is distributed under the BSD 3-Clause License. See LICENSE for details.

#ifndef INOU_RAND_H
#define INOU_RAND_H

#include <string>

#include "inou.hpp"
#include "py_options.hpp"

class Inou_rand_options : public Py_options {
public:
  int         rand_seed;
  int         rand_size;
  int         rand_crate;
  double      rand_eratio;

  Inou_rand_options() {
     rand_seed   = std::rand();
     rand_size   = 8192;
     rand_crate  = 10;
     rand_eratio = 4;
  }
  void set(const py::dict &dict) final;
};

class Inou_rand : public Inou {
private:
protected:
  Inou_rand_options opack;

public:
  Inou_rand();

  std::vector<LGraph *> generate() final;
  using Inou::generate;
  void generate(std::vector<const LGraph *> &out) final;

  // Python interface
  Inou_rand(const py::dict &dict);

  std::vector<LGraph *> py_generate() { return generate(); };
  void py_set(const py::dict &dict);
};

#endif
