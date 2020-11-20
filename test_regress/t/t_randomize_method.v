// DESCRIPTION: Verilator: Verilog Test module
//
// This file ONLY is placed under the Creative Commons Public Domain, for
// any use, without warranty, 2020 by Wilson Snyder.
// SPDX-License-Identifier: CC0-1.0

typedef enum {
   ONE   = 3,
   TWO   = 5,
   THREE = 8,
   FOUR  = 13
} Enum;

typedef struct packed {
   int  a;
   bit  b;
   Enum c;
} StructInner;

typedef struct packed {
   bit         x;
   StructInner s;
   Enum        y;
   longint     z;
} StructOuter;

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
   rand Enum l;

   function new;
      i = new;
      j = 0;
      k = 0;
      l = ONE;
   endfunction

endclass

class DerivedB extends Base;
   logic[63:0] v;
   rand logic[63:0] w;
   rand logic[47:0] x;
   rand logic[31:0] y;
   rand logic[23:0] z;
   rand StructOuter str;

   function new;
      v = 0;
      w = 0;
      x = 0;
      y = 0;
      z = 0;
      str = '{x: 1'b0, y: ONE, z: 64'd0, s: '{a: 32'd0, b: 1'b0, c: ONE}};
   endfunction

endclass

module t (/*AUTOARG*/);
   bit ok = 0;
   longint checksum;

   task checksum_next(longint x);
      checksum = x ^ {checksum[62:0],checksum[63]^checksum[2]^checksum[0]};
   endtask;

   DerivedA derivedA;
   DerivedB derivedB;
   Base base;

   initial begin
      int rand_result;
      longint prev_checksum;
      derivedA = new;
      derivedB = new;
      base = derivedA;
      for (int i = 0; i < 10; i++) begin
         checksum = 0;
         rand_result = base.randomize();
         rand_result = derivedB.randomize();
         if (!(derivedA.l inside {ONE, TWO, THREE, FOUR})) $stop;
         if (!(derivedB.str.s.c inside {ONE, TWO, THREE, FOUR})) $stop;
         if (!(derivedB.str.y inside {ONE, TWO, THREE, FOUR})) $stop;
         if (derivedA.i.e != 0) $stop;
         if (derivedA.k != 0) $stop;
         if (derivedB.v != 0) $stop;
         checksum_next(longint'(derivedA.i.a));
         checksum_next(longint'(derivedA.i.b));
         checksum_next(longint'(derivedA.i.c));
         checksum_next(longint'(derivedA.j));
         checksum_next(longint'(derivedA.l));
         checksum_next(longint'(derivedB.w));
         checksum_next(longint'(derivedB.x));
         checksum_next(longint'(derivedB.y));
         checksum_next(longint'(derivedB.z));
         checksum_next(longint'(derivedB.str.x));
         checksum_next(longint'(derivedB.str.y));
         checksum_next(longint'(derivedB.str.z));
         checksum_next(longint'(derivedB.str.s.a));
         checksum_next(longint'(derivedB.str.s.b));
         checksum_next(longint'(derivedB.str.s.c));
         $write("checksum: %d\n", checksum);
         if (i > 0 && checksum != prev_checksum) begin
            ok = 1;
         end
         prev_checksum = checksum;
      end
      if (ok) begin
         $write("*-* All Finished *-*\n");
         $finish;
      end
      else $stop;
   end
endmodule
