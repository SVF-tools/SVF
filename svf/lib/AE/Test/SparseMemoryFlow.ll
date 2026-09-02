; Memory refinement fixture. The first load drives a branch refinement of the
; backing ObjVar. The second load must observe both the reaching store and the
; branch constraint, so %memory_result has lower bound one in every mode.

define i32 @main(i32 %input) {
entry:
  %cell = alloca i32, align 4
  store i32 %input, ptr %cell, align 4
  %first = load i32, ptr %cell, align 4
  %positive = icmp sgt i32 %first, 0
  br i1 %positive, label %positive_path, label %exit

positive_path:
  %second = load i32, ptr %cell, align 4
  %memory_result = add nsw i32 %second, 0
  ret i32 %memory_result

exit:
  ret i32 0
}
