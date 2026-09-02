; End-to-end reduced-product fixture. Interval AE alone does not propagate
; branch bounds between %x and its affine copy %y. The Octagon component does.

define i32 @main(i32 %x, ptr %argv) {
entry:
  %nonnegative = icmp sge i32 %x, 0
  br i1 %nonnegative, label %upper_check, label %exit

upper_check:
  %at_most_ten = icmp sle i32 %x, 10
  br i1 %at_most_ten, label %bounded, label %exit

bounded:
  %y = add nsw i32 %x, 0
  %impossible = icmp slt i32 %x, %y
  br i1 %impossible, label %unreachable, label %small_check

unreachable:
  %bad = add nsw i32 %x, 100
  ret i32 %bad

small_check:
  %small = icmp sle i32 %x, 5
  br i1 %small, label %reduced, label %exit

reduced:
  %z = add nsw i32 %y, 1
  ret i32 %z

exit:
  ret i32 0
}
