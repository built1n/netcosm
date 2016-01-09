/* functions allowing a child process to send its master requests */

extern struct userdata_t sent_userdata;
extern bool child_req_success;

void client_change_room(room_id id);
void client_change_state(int state);
void client_change_user(const char *user);
void client_move(const char *dir);
void client_look(void);

void send_master(unsigned char cmd, const void *data, size_t sz);
