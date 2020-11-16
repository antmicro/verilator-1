// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty, 2020 by Wilson Snyder.
// SPDX-License-Identifier: CC0-1.0

class Base;
endclass

class Inner;
   rand logic[7:0] a;
   rand logic[15:0] b;
   rand logic[3:0] c;
   rand logic[11:0] d;
   int e;

   function new;
      a = 0;
      b = 0;
      c = 0;
      d = 0;
      e = 0;
   endfunction

endclass

class DerivedA extends Base;
   rand Inner i;
   rand int j;
   int k;

function new;
      i = new;
      j = 0;
      k = 0;
   endfunction

endclass

class DerivedB extends Base;
   logic[63:0] v;
   rand logic[63:0] w;
   rand logic[47:0] x;
   rand logic[31:0] y;
   rand logic[23:0] z;

   function new;
      v = 0;
      w = 0;
      x = 0;
      y = 0;
      z = 0;
   endfunction

endclass

module t (/*AUTOARG*/);
   DerivedA derivedA;
   DerivedB derivedB;
   Base base;

   initial begin
      int rand_result;
      derivedA = new;
      derivedB = new;
      base = derivedA;
      rand_result = base.randomize();
      rand_result = derivedB.randomize();
      if (derivedA.i.a == 0) $stop;
      if (derivedA.i.b == 0) $stop;
      if (derivedA.i.c == 0) $stop;
      if (derivedA.i.d == 0) $stop;
      if (derivedA.i.e != 0) $stop;
      if (derivedA.j == 0) $stop;
      if (derivedA.k != 0) $stop;
      if (derivedB.v != 0) $stop;
      if (derivedB.w == 0) $stop;
      if (derivedB.x == 0) $stop;
      if (derivedB.y == 0) $stop;
      if (derivedB.z == 0) $stop;
      $write("*-* All Finished *-*\n");
      $finish;
   end
endmodule
