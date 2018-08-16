#include <stdio.h>
#include <stdint.h>
#include <glib.h>
#include <gst/gst.h>
#include <signal.h>
#include <getopt.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

static GMainLoop *g_loop = NULL;

static bool g_quit = false;

static gboolean bus_call(GstBus *bus, GstMessage *msg,gpointer data){
    GMainLoop *loop = (GMainLoop *)data;
    switch(GST_MESSAGE_TYPE (msg)){
        case GST_MESSAGE_EOS:
            g_print("End of stream\n");
            g_main_loop_quit(loop);
            break;
        case GST_MESSAGE_ERROR: {
            gchar  *debug;
            GError *error;
            gst_message_parse_error(msg, &error, &debug);
            g_free(debug);
            g_printerr("Error: %s\n", error->message);
            g_error_free(error);
            g_main_loop_quit(loop);
            break;
        }
        default:
            break;
    }
    return TRUE;
}

static void usage(FILE *fp, int argc, char **argv){
    fprintf (fp,
             "Usage: %s [options]\n\n"
             "Options:\n"
             "  -e | --endianness   endianness of datas[ 1234 , 4321 ] \n"
             "  -s | --signed       signed of datas[ true , false ]\n"
             "  -w | --width        width of datas\n"
             "  -d | --depth        depth of datas\n"
             "  -c | --channels     channels\n"
             "  -r | --rate         rate\n"
             "  -p | --path         filepath (default: ./test.pcm)\n"
             "  -h | --help         print this message\n\n"
             "-- No input or only input path parameters will be recorded with default parameters of alsasrc. --\n\n"
             "",
             basename(argv[0]));
}

static const char short_options [] = "e:w:d:c:r:s:p:h";

static const struct option long_options [] = {
    { "endianness",     required_argument,      NULL,           'e' },
    { "signed",         required_argument,      NULL,           's' },
    { "width",      	required_argument,      NULL,           'w' },
    { "depth",          required_argument,      NULL,           'd' },
    { "channels",    	required_argument,      NULL,           'c' },
    { "rate",           required_argument,      NULL,           'r' },
    { "path",       	required_argument,      NULL,           'p' },
    { "help",       	no_argument,            NULL,           'h' },
    { 0, 0, 0, 0 }
};

static void *timing_thread(void *){
    static struct timeval t_start, t_end;
    int duration = 0;

    gettimeofday(&t_start, NULL);

    printf("Ready for starting\n\n");

    while(!g_quit){
        gettimeofday(&t_end, NULL);
        duration = (t_end.tv_sec - t_start.tv_sec);

        printf("-- Recording %02d:%02d:%02d --", duration/3600, duration/60, duration%60);
        fflush(stdout);

        printf("\r\033[k");

        sleep(1);
    }

    printf("Stoping\n");

    return (void *)0;
}

static void stop(int){
    printf("\n\n\r\033[k");
    g_main_loop_quit(g_loop);
    g_quit = true;
    sleep(1);
}

int main(int argc, char **argv){
    signal(SIGINT, stop);
    pthread_t id;

    int endianness = 1234;
    int width = 32;
    int depth = 32;
    int channels = 2;
    int rate = 44100;
    bool sign = true;
    char *path = (char*)"./test.pcm";
    bool path_set = false;

    for(;;){
        int index;
        int c;

        c = getopt_long(argc, argv,short_options, long_options,&index);

        if(-1 == c)
            break;

        switch(c){
            case 0:
                    exit(EXIT_FAILURE);
            case 'e':
                    endianness = atoi(optarg);
                    break;
            case 'w':
                    width = atoi(optarg);
                    break;
            case 'd':
                    depth = atoi(optarg);
                    break;
            case 'c':
                    channels = atoi(optarg);
                    break;
            case 'r':
                    rate = atoi(optarg);
                    break;
            case 's':
                    if(strcmp("true",optarg) == 0)
                        sign = true;
                    else if(strcmp("false",optarg) == 0)
                        sign = false;
                    else
                        usage(stdout, argc, argv);
                    break;
            case 'p':
                    path = optarg;
                    path_set = true;
                    break;
            case 'h':
                    usage(stdout, argc, argv);
                    exit(EXIT_SUCCESS);
            default:
                    usage(stderr, argc, argv);
                    exit(EXIT_FAILURE);
        }
    }

    GstElement *pipeline, *source, *queue, *sink;
    GstBus *bus;
    GstCaps *caps;
    gboolean ret;

    /* Initialization of gstreamer */
    gst_init(&argc, &argv);
    g_loop = g_main_loop_new(NULL, FALSE);

    /* Create a new pipeline */
    pipeline = gst_pipeline_new("_pipeline");

    /* Initialization of elements */
    source = gst_element_factory_make("alsasrc", "_source");
    queue  = gst_element_factory_make("queue", "_queue");
    sink   = gst_element_factory_make("filesink", "_sink");

    if(!pipeline || !source || !queue || !sink) {
        g_printerr("Element Creation failure.\n");
        /* Release gst pipeline*/
        gst_object_unref(GST_OBJECT(pipeline));
        /* Quit loop */
        g_main_loop_unref(g_loop);
        return -1;
    }

    /* Set elements params */
    g_object_set(G_OBJECT(source), "device", "hw:0,0", NULL);
    g_object_set(G_OBJECT(sink), "location", path, NULL);

    gst_bin_add_many(GST_BIN(pipeline), source, queue, sink, NULL);

    if((argc == 3 && path_set) || argc == 1 ) {
        gst_element_link_many(source, queue, sink,NULL);
    }else{
        /* audio/x-raw-int, endianness=1234, signed=true, width=32, depth=32, rate=44100, channels=2 */
        caps = gst_caps_new_simple("audio/x-raw-int",
                                   "endianness", G_TYPE_INT, endianness,
                                   "signed", G_TYPE_BOOLEAN, sign,
                                   "width", G_TYPE_INT, width,
                                   "depth", G_TYPE_INT, depth,
                                   "rate", G_TYPE_INT, rate,
                                   "channels", G_TYPE_INT, channels,
                                   NULL);

        /* Elements link with caps filter.(source -> queue) */
        ret = gst_element_link_filtered(source, queue, caps);

        /* Release caps */
        gst_caps_unref(caps);

        if(!ret){
            g_warning ("Caps filter link failed.(source -> queue)");
            /* Release gst pipeline*/
            gst_object_unref(GST_OBJECT(pipeline));
            /* Quit loop */
            g_main_loop_unref(g_loop);
            return -1;
        }

        /* Elements link with pads.(queue -> sink) */
        ret = gst_element_link_pads (queue, "src", sink, "sink");

        if(!ret){
            g_warning("Element link pads failed.(queue -> sink)");
            /* Release gst pipeline*/
            gst_object_unref(GST_OBJECT(pipeline));
            /* Quit loop */
            g_main_loop_unref(g_loop);
            return -1;
        }
    }

    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    gst_bus_add_watch(bus, bus_call, g_loop);
    gst_object_unref(bus);

    /* Strat */
    ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_ASYNC || ret == GST_STATE_CHANGE_SUCCESS){
        if(pthread_create(&id, NULL, timing_thread, NULL)){
            printf("Creating timing_thread failed !\n");
            /* Release gst pipeline*/
            gst_object_unref(GST_OBJECT(pipeline));
            /* Quit loop */
            g_main_loop_unref(g_loop);
            return -1;
        }
        pthread_detach(id);
    }

    g_main_loop_run(g_loop);

    /* Exit the timing thread */
    g_quit = true;

    /* Release gst pipeline*/
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline));

    /* Quit loop */
    g_main_loop_unref(g_loop);

    printf("Exit\n");

    return 0;
}
