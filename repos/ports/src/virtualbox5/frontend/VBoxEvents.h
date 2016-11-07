#include "EventImpl.h"

void fireStateChangedEvent(IEventSource* aSource, MachineState_T a_state);

void fireRuntimeErrorEvent(IEventSource* aSource, BOOL a_fatal, CBSTR a_id,
                           CBSTR a_message);

#define fireAdditionsStateChangedEvent(a)

#define fireBandwidthGroupChangedEvent(a, b)

#define fireClipboardModeChangedEvent(a, b)
#define fireCPUChangedEvent(a, b, c)
#define fireCPUExecutionCapChangedEvent(a, b)

#define fireDnDModeChangedEvent(a, b)

#define fireExtraDataChangedEvent(a, b, c, d)

#define fireGuestUserStateChangedEvent(a, b, c, d, e)
#define fireGuestMonitorChangedEvent(a, b, c, d, e, f, g)

#define fireHostNameResolutionConfigurationChangeEvent(a)
#define fireHostPCIDevicePlugEvent(a, b, c, d, e, f)

#define fireKeyboardLedsChangedEvent(a, b, c, d)

#define fireMediumChangedEvent(a, b)
#define fireMousePointerShapeChangedEvent(a, b, c, d, e, f, g, h)
#define fireMouseCapabilityChangedEvent(a, b, c, d, e)

#define fireNATRedirectEvent(a, b, c, d, e, f, g, h, i, j)
#define fireNATNetworkChangedEvent(a, b)
#define fireNATNetworkPortForwardEvent(a, b, c, d, e, f, g, h, i, j)
#define fireNATNetworkSettingEvent(a, b, c, d, e, f, g)
#define fireNATNetworkStartStopEvent(a, b, c)
#define fireNetworkAdapterChangedEvent(a, b)

#define fireParallelPortChangedEvent(a, b)

#define fireSerialPortChangedEvent(a, b)
#define fireSharedFolderChangedEvent(a, b)
#define fireStorageControllerChangedEvent(a)
#define fireStorageDeviceChangedEvent(a, b, c, d)

#define fireUSBControllerChangedEvent(a)
#define fireUSBDeviceStateChangedEvent(a, b, c, d)

#define fireVideoCaptureChangedEvent(a)
#define fireVRDEServerChangedEvent(a)
#define fireVRDEServerInfoChangedEvent(a)
