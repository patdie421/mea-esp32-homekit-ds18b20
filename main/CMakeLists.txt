# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "contacts.c tcp_server.c relays.c temperature_ds18b20.c status_led.c config.c network.c main.c")
set(COMPONENT_ADD_INCLUDEDIRS "")

register_component()
