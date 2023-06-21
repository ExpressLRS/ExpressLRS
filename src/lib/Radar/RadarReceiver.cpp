#if defined(USE_RADAR)

static

void RadarRx_Begin()
{
    wifi_promiscuous_enable(0);
    wifi_set_promiscuous_rx_cb(promisc_cb);
    wifi_promiscuous_enable(1);
}

void RadarRx_End()
{

}
#endif