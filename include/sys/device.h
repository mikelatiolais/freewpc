#ifndef _SYS_DEVICE_H
#define _SYS_DEVICE_H

/** The maximum number of switches that a ball device can have */
#ifndef MAX_SWITCHES_PER_DEVICE
#define MAX_SWITCHES_PER_DEVICE 6
#endif

/** The maximum number of ball devices that are supported */
#ifndef MAX_DEVICES
#define MAX_DEVICES 8
#endif

/** The device number for the trough device, which all games must have */
#define DEV_TROUGH 0

#if 0
#define MAX_BALLS 6
#endif

#define DEVICE_GID_BASE		GID_DEVICE0_ACTIVE

#define DEVICE_GID(devno)	(GID_DEVICE0_ACTIVE + (devno))

struct device;

/** The device operations structure.  Each device can define callbacks
 * for various types of events/operations that are device-specific.
 * You can leave a field as NULL and it will not be called.
 * These are game-specific functions, as all of the basic stuff is done
 * by the common code here. */
typedef struct device_ops
{
	/** Called when the machine is powered up */
	void (*power_up) (struct device *dev);

	/** Called whenever a game is started */
	void (*game_start) (struct device *dev);

	/** Called whenever a ball enters the device */
	void (*enter) (struct device *dev);

	/** Called whenever the game tries to kick a ball from the device */
	void (*kick_attempt) (struct device *dev);

	/** Called when a kick is successful */
	void (*kick_success) (struct device *dev);

	/** Called when a kick is not successful */
	void (*kick_failure) (struct device *dev);

	/** Called when the device becomes full */
	void (*full) (struct device *dev);

	/** Called when the device becomes empty */
	void (*empty) (struct device *dev);
} device_ops_t;


#define device_call_op(dev, op) \
do { \
	dbprintf ("Calling device hook %s\n", #op); \
	if (in_live_game && (dev->props->ops->op)) \
	{ \
		(*dev->props->ops->op) (dev); \
	} \
} while (0)


/** The device structure is a read-only descriptor that
 * contains various device properties. */
typedef struct device_properties
{
	device_ops_t *ops;
	const char *name;
	solnum_t sol;
	uint8_t sw_count;
	uint8_t init_max_count;
	switchnum_t sw[MAX_SWITCHES_PER_DEVICE];
} device_properties_t;


/**
 * Device states.  Each device is governed by a state
 * machine; these values dictate the various states that
 * a device can be in.
 */
#define DEV_STATE_IDLE			0
#define DEV_STATE_ENTERED		1
#define DEV_STATE_RELEASING	2

/** The device info structure.  This is a read-write
 * structure that maintains the current state of a device.
 * Included is the state machine state, as well as other
 * properties like how many balls are currently in the
 * device. */
typedef struct device
{
	uint8_t devno;
	uint8_t size;
	uint8_t actual_count;
	uint8_t previous_count;
	uint8_t max_count;
	uint8_t kicks_needed;
	uint8_t state;
	device_properties_t *props;
} device_t;

typedef uint8_t devicenum_t;

#define device_entry(devno)	(&device_table[devno])
#define device_devno(dev)		(dev->devno)

#define device_empty_p(dev)	(dev->actual_count == 0)
#define device_full_p(dev)		(dev->actual_count == dev->max_count)

#define device_disable_lock(dev)	(dev->max_count--)
#define device_enable_lock(dev)	(dev->max_count++)

extern device_t device_table[];

void device_clear (device_t *dev);
void device_register (devicenum_t devno, device_properties_t *props);
uint8_t device_recount (device_t *dev);
void device_update_globals (void);
void device_probe (void);
void device_request_kick (device_t *dev);
void device_request_empty (device_t *dev);
void device_sw_handler (uint8_t devno);
void device_add_live (void);
void device_remove_live (void);
void device_multiball_set (U8 count);
void device_init (void);

void trough_init (void);

#endif /* _SYS_DEVICE_H */
