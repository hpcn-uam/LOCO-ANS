
.PHONY: clean

get_peak_error: get_peak_error.cpp
	g++ get_peak_error.cpp `pkg-config opencv --libs` -o get_peak_error

clean:
	rm -f get_peak_error