#define this_node_id 4 // remote node must have node_id > 1 (1 is the gateway)
const char* loranet_pubkey = "d5jahi4w8rvm"; // gateway's public key
const char* this_node_pubkey = "d5jahi4w8rvm"; // pub key for this node (same as above if we're node #1 / gateway)
const char* this_node_privkey = "56jpn44sw7fh"; // priv key for this node

const int interval_sec = 20;
const int random_interval_sec = 3; // range of random seconds to add to interval_sec
const int forcePPM = 400;
