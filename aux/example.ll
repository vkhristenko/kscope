@G = weak global i32 0
@H = weak global i32 0

define i32 @test(i1 %Condition) {
entry:
    %X = alloca i32           ; type of %X is i32*.
    br i1 %Condition, label %cond_true, label %cond_false
}
