#include "EventImpl.h"

void fireStateChangedEvent(IEventSource* aSource, MachineState_T a_state);

void fireRuntimeErrorEvent(IEventSource* aSource, BOOL a_fatal, CBSTR a_id,
                           CBSTR a_message);

void fireGuestMonitorChangedEvent(IEventSource* aSource,
                                  GuestMonitorChangedEventType_T a_changeType,
                                  ULONG a_screenId, ULONG a_originX,
                                  ULONG a_originY, ULONG a_width,
                                  ULONG a_height);

void fireHostPCIDevicePlugEvent(IEventSource* aSource, CBSTR a_machineId,
                                BOOL a_plugged, BOOL a_success,
                                IPCIDeviceAttachment* a_attachment,
                                CBSTR a_message);

void fireNATRedirectEvent(IEventSource* aSource, CBSTR a_machineId,
                          ULONG a_slot, BOOL a_remove, CBSTR a_name,
                          NATProtocol_T a_proto, CBSTR a_hostIP,
                          LONG a_hostPort, CBSTR a_guestIP, LONG a_guestPort);

void fireNATNetworkChangedEvent(IEventSource* aSource, CBSTR a_networkName);

void fireNATNetworkStartStopEvent(IEventSource* aSource, CBSTR a_networkName,
                                  BOOL a_startEvent);

void fireNATNetworkSettingEvent(IEventSource* aSource, CBSTR a_networkName,
                                BOOL a_enabled, CBSTR a_network,
                                CBSTR a_gateway,
                                BOOL a_advertiseDefaultIPv6RouteEnabled,
                                BOOL a_needDhcpServer);

void fireNATNetworkPortForwardEvent(IEventSource* aSource, CBSTR a_networkName,
                                    BOOL a_create, BOOL a_ipv6, CBSTR a_name,
                                    NATProtocol_T a_proto, CBSTR a_hostIp,
                                    LONG a_hostPort, CBSTR a_guestIp,
                                    LONG a_guestPort);

void fireHostNameResolutionConfigurationChangeEvent(IEventSource* aSource);

void fireMediumChangedEvent(IEventSource *, IMediumAttachment *);

#define fireGuestPropertyChangedEvent(a, b, c, d, e)
