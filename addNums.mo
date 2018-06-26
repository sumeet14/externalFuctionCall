function addNums
  input Integer num1;
  input Integer num2;
  output Integer out;

  external "C" out = addNumbers(num1, num2);
  annotation(Include = "#include \"external.h\"", Library = "external");
end addNums;
