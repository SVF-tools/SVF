; Regression for conservative handling of truncation targets wider than i32.

define i64 @main(i32 %argc, ptr %argv) {
entry:
  %wide_result = trunc i128 42 to i64
  ret i64 %wide_result
}
