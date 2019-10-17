DECISION      := decision.out

$(DECISION): $(TEST_EXE) all
	$(PIN_ROOT)/pin -t ../PinInstrument/obj-intel64/PinInstrument.so -o $@ -- ./$(TEST_EXE)