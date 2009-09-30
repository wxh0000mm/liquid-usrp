//
//
//

#include <iostream>
#include <math.h>
#include <complex>
#include <liquid/liquid.h>
#include "usrp_io.h"

#define USRP_CHANNEL    (0)

void * tx_handler( void * _port );
void * rx_handler( void * _port );

int main() {
    // options
    float   tx_freq     = 462e6f;
    float   rx_freq     = 462.5625e6f;
    int     tx_interp   = 512;
    int     rx_decim    = 256;

    // create usrp object
    usrp_io * usrp = new usrp_io();

    // set properties
    usrp->set_tx_freq(USRP_CHANNEL, tx_freq);
    usrp->set_tx_interp(tx_interp);
    usrp->set_rx_freq(USRP_CHANNEL, rx_freq);
    usrp->set_rx_decim(rx_decim);
    usrp->enable_auto_tx(USRP_CHANNEL);

    // ports
    gport port_tx = usrp->get_tx_port(USRP_CHANNEL);
    gport port_rx = usrp->get_rx_port(USRP_CHANNEL);

    // threads
    pthread_t tx_thread;
    pthread_t rx_thread;
    pthread_attr_t thread_attr;
    void * status;
    
    // set thread attributes
    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);

    // attributes object no longer needed
    pthread_attr_destroy(&thread_attr);

    std::cout << "waiting to start threads..." << std::endl;
    usleep(2000000);

    // create threads
    pthread_create(&tx_thread, &thread_attr, &tx_handler, (void*) port_tx);
    pthread_create(&rx_thread, &thread_attr, &rx_handler, (void*) port_rx);

    // start data transfer
    usrp->start_tx(USRP_CHANNEL);
    usrp->start_rx(USRP_CHANNEL);

    std::cout << "waiting for threads to exit..." << std::endl;

    // join threads
    pthread_join(tx_thread, &status);
    pthread_join(rx_thread, &status);

    // stop
    usrp->stop_rx(USRP_CHANNEL);
    usrp->stop_tx(USRP_CHANNEL);

    printf("main process complete\n");

    // delete usrp object
    delete usrp;
}

void * tx_handler ( void *_port )
{
    gport port = (gport) _port;
 
    // interpolator options
    unsigned int m=4;
    float beta=0.3f;
    unsigned int h_len = 2*2*m + 1;
    float h[h_len];
    design_rrc_filter(2,m,beta,0,h);
    interp_crcf interp = interp_crcf_create(2,h,h_len);

    unsigned int num_symbols=256;
    std::complex<float> s[num_symbols];
    std::complex<float> * x;
    unsigned int i;
    printf("tx thread running...\n");
    for (unsigned int n=0; n<2000; n++) {
    //while (1) {
        // get data from port
        x = (std::complex<float>*) gport_producer_lock(port,2*num_symbols);
        for (i=0; i<num_symbols; i++) {
            s[i].real() = rand()%2 ? 1.0f : -1.0f;
            s[i].imag() = rand()%2 ? 1.0f : -1.0f;
            interp_crcf_execute(interp, s[i], &x[2*i]);
        }

        //printf("releasing port...\n");

        // release port
        gport_producer_unlock(port,2*num_symbols);
    }
    std::cout << "done." << std::endl;
    interp_crcf_destroy(interp);
   
    printf("tx_handler finished.\n");
    pthread_exit(0); // exit thread
}


void * rx_handler ( void *_port )
{
    gport p = (gport) _port;

    std::complex<float> * data_rx;
    std::complex<float> spectrogram_buffer[64];

    asgram sg = asgram_create(spectrogram_buffer,64);
    asgram_set_scale(sg,20.0f);
    asgram_set_offset(sg,80.0f);

    for (unsigned int n=0; n<4000; n++) {
        data_rx = (std::complex<float>*) gport_consumer_lock(p,512);
        
        // run ascii spectrogram
        if (n%30 == 0) {
            memmove(spectrogram_buffer, data_rx, 64*sizeof(std::complex<float>));
            asgram_execute(sg);
        }

        gport_consumer_unlock(p,512);
    }

    printf("rx_handler finished.\n");
    pthread_exit(0); // exit thread
}


