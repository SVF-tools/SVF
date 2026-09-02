; Bounded-loop fixture for the unified program-state lifecycle.  The cycle
; forces AE through widening and narrowing; the exit branch checks that the
; resulting interval and Octagon components agree that %i is exactly four.

define i32 @main() {
entry:
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %next, %body ]
  %continue = icmp slt i32 %i, 4
  br i1 %continue, label %body, label %exit

body:
  %next = add nsw i32 %i, 1
  br label %loop

exit:
  %too_small = icmp slt i32 %i, 4
  br i1 %too_small, label %unreachable, label %done

unreachable:
  %loop_bad = add nsw i32 %i, 100
  ret i32 %loop_bad

done:
  %loop_result = add nsw i32 %i, 0
  ret i32 %loop_result
}
