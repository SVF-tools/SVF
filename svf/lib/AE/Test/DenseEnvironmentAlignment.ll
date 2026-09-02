; Cross-function fixture for state-local environment growth.  Processing the
; call introduces callee variables into a caller state after its fixpoint
; snapshot was taken, so native Dense AE must align environments before state
; equivalence checks.

define i32 @callee(i32 %callee_x) {
entry:
  %callee_y = add nsw i32 %callee_x, 1
  ret i32 %callee_y
}

define i32 @main(i32 %caller_x, ptr %argv) {
entry:
  %env_result = call i32 @callee(i32 %caller_x)
  %env_condition = icmp sgt i32 %env_result, 0
  br i1 %env_condition, label %positive, label %non_positive

positive:
  br label %merge

non_positive:
  br label %merge

merge:
  %env_phi = phi i32 [ %env_result, %positive ], [ 0, %non_positive ]
  ret i32 %env_phi
}
