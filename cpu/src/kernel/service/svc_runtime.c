

typedef enum {
	FT_EVENT_BUTTON = 0,
	FT_EVENT_ENCODER = 1,
	FT_EVENT_KNOB = 2,
	FT_EVENT_UNDEFINED = 3,
	FT_EVENT_TRIGGER = 4,
	FT_EVENT_XY_PAD = 5,
	FT_EVENT_PANEL_ACK,
	FT_EVENT_HELD_BUTTONS // Only fires once HELD_BUTTONS_MSW received.
} ft_event_id_t;

typedef struct {
	ft_event_id_t id;
} ft_event_t;

void ft_subscribe_event(ft_event_id_t event_id);

bool ft_poll_event(ft_event_t *event);

typedef enum {
	FT_PRIO_BACKGROUND,
	FT_PRIO_NORMAL,
	FT_PRIO_REALTIME
} ft_priority_t;

typedef void (*ft_task_fn)(void);

void ft_create_task(ft_task_fn *proc, ft_priority_t prio);


