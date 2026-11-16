/* test script: load a step state, picture it, run forward (Up, bit 4) for three seconds, picture it again: Lara and the way
   on the map should have moved with her. Then drag the map and picture it. */
#define TEST_SCRIPT {0, 0, 0, -1, -1}, {80, 180, 1u << 4, -1, -1}
#define TEST_STATE "sdmc:/3ds/tomb-raider-dual/test.state"
#define TEST_SHOT 60, 270
#define TEST_QUIT 290
