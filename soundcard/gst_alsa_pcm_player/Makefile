SOURCE  := $(wildcard *.c) $(wildcard *.cpp)
OBJS    := $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCE)))

GST_VER := 1.X
TARGET  := gst_alsa_pcm_player

CC      := gcc
CXX     := g++
DEFINES := 
INCLUDE :=

ifeq ($(GST_VER),1.X) 
CFLAGS  += -g -Wall -O3 $(DEFINES) $(INCLUDE) `pkg-config gstreamer-1.0 --cflags` `pkg-config glib-2.0 --cflags`
LIBS    += `pkg-config gstreamer-1.0 --libs` `pkg-config gstreamer-app-1.0 --libs` -lpthread
else
CFLAGS  += -g -Wall -O3 $(DEFINES) $(INCLUDE) `pkg-config gstreamer-0.10 --cflags` `pkg-config glib-2.0 --cflags`
LIBS    += `pkg-config gstreamer-0.10 --libs` `pkg-config gstreamer-app-0.10 --libs` `pkg-config glib-2.0 --libs` -lpthread
endif 

LDFLAGS :=
CXXFLAGS:= $(CFLAGS) -DHAVE_CONFIG_H
  
.PHONY : everything objs clean distclean rebuild
  
all : $(TARGET)
  
objs : $(OBJS)
  
rebuild: distclean all
                
clean :
	rm -rf *~
	rm -rf *.o
    
distclean : clean
	rm -rf $(TARGET)
  
$(TARGET) : $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

