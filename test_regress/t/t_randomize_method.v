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
         checksum_next(longint'(derivedA.i.a));
         checksum_next(longint'(derivedA.i.b));
         checksum_next(longint'(derivedA.i.c));
         checksum_next(longint'(derivedA.i.d));
         checksum_next(longint'(derivedA.i.e));
         checksum_next(longint'(derivedA.j));
         checksum_next(longint'(derivedA.k));
         checksum_next(longint'(derivedB.v));
         checksum_next(longint'(derivedB.w));
         checksum_next(longint'(derivedB.x));
         checksum_next(longint'(derivedB.y));
         checksum_next(longint'(derivedB.z));
         $write("checksum: %d\n", checksum);
         if (i > 0 && checksum != prev_checksum) begin
            ok = 1;
            break;
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
