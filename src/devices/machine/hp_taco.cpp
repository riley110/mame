// license:BSD-3-Clause
// copyright-holders:F. Ulivi
/*********************************************************************

    hp_taco.cpp

    HP TApe COntroller (5006-3012)

*********************************************************************/

// This device has been reverse engineered entirely through documents & study of HP software.
// I had no access to the real device to experiment.
// Available documentation on the internal working of TACO chip is close to nothing. The best
// I could find is [1] (see below) where all that's described is a (too) brief summary of registers and little else.
// In other words, no description of the commands that can be issued to TACO chips.
// So, my main source of information was the careful study of HP software, especially the 9845 system test ROM (09845-66520).
// The second half of this ROM holds a comprehensive set of tape drive tests.
// The main shortcomings of my approach are:
// * I could indentify only those TACO commands that are actually used by the software. I managed
//   to identify 17 out of 32 possible commands. The purpose of the rest of commands is anyone's guess.
// * I could only guess the behavior of TACO chips in corner cases (especially behavior in various error/abnormal
//   conditions)
//
// Documentation I used:
// [1]  HP, manual 64940-90905, may 80 rev. - Model 64940A tape control & drive service manual
// [2]  US patent 4,075,679 describing HP9825 system (this system had a discrete implementation of tape controller). The
//      firmware listing was quite useful in identifying sequences of commands (for example how to find a specific sector etc.).
// [3]  http://www.hp9845.net site
// [4]  April 1978 issue of HP Journal. There is a one-page summary of TACO chip on page 20.

// This is an overview of the TACO/CPU interface.
//
// Reg. | R/W | Content
// =====================
// R4   | R/W | Data register: words read/written to/from tape pass through this register
// R5   | R/W | Command and status register (see below)
// R6   | R/W | Tachometer register. Writing it sets a pulse counter that counts up on either tachometer pulses or IRGs, depending
//      |     | on command. When the counter rolls over from 0xffff to 0 it typically ends the command. It's not clear to me
//      |     | what value could be read from this register, if it's just the same value that was written last time or the internal
//      |     | counter or something else entirely.
// R7   | R   | Checksum register. Reading it clears it.
// R7   | W   | Timing register. It controls somehow the encoding and decoding of bits. For now I completely ignore it because its
//      |     | content it's totally unknown to me. It seems safe to do so, anyway. I can see that it's always set to 0x661d before
//      |     | writing to tape and to 0x0635 before reading.
//
// Format of TACO command/status register (R5)
// Bit    R/W Content
// ===============
// 15     RW  Tape direction (1 = forward)
// 14..10 RW  Command (see the "enum" below)
//  9     RW  ? Drive ON according to [1], the actual use seems to be selection of gap length
//  8     RW  ? Size of gaps according to [1], N/U in my opinion
//  7     RW  Speed of tape (1 = 90 ips, 0 = 22 ips)
//  6     RW  Option bit for various commands
//  5     R   Current track (1 = B)
//  4     R   Gap detected (1)
//  3     R   Write protection (1)
//  2     R   Servo failure (1)
//  1     R   Cartridge out (1)
//  0     R   Hole detected (1)

// Here's a summary of the on-tape format of HP9845 systems.
// * A tape has two independent tracks (A & B).
// * Each track holds 426 sectors.
// * Each sector has an header and 256 bytes of payload (see below)
// * Sectors are separated by gaps of uniform magnetization called IRGs (Inter-Record Gaps)
// * The basic unit of data I/O are 16-bit words
// * Bits are encoded by different distances between magnetic flux reversals
// * The structure of tracks is:
//   - Begin of tape holes
//   - The deadzone: 350x 0xffff words
//   - 1" of IRG
//   - Sector #0 (track A) or #426 (track B)
//   - 1" of IRG (2.5" on track A)
//   - Sector #1 (track A) or #427 (track B)
//   - 1" of IRG
//   - Sector #2 (track A) or #428 (track B)
//   - ...and so on up to sector #425/#851
//   - 6" of final gap
//   - Non-recorded tape
//   - End of tape holes
// * Sector #0 is not used
// * Sectors #1 and #2 hold the first copy of tape directory
// * Sectors #3 and #4 hold the second/backup copy of tape directory
// * User data are stored starting from sector #5
// * There is no "fragmentation" map (like file allocation table in FAT filesystem): a file
//   spanning more than 1 sector always occupy a single block of contiguous sectors.
//
// A sector is structured like this:
// Word 0:      Invisible preamble word (always 0). Preamble comes from 9825, don't know if it's
//              actually there in TACO encoding. I assumed it is.
// Word 1:      Format/sector in use and other unidentified bits.
// Word 2:      Sector number
// Word 3:      Sector length and other bits
// Word 4:      Checksum (sum of words 1..3)
// Words 5..132:        Payload
// Word 133:    Checksum (sum of words 5..132)
//
// Physical encoding of words is borrowed from 9825 as I wasn't able
// to gather any info on the actual encoding of TACO chips.
// This is how 9825 encodes words on tape:
// - the unit of encoding are 16-bit words
// - each word is encoded from MSB to LSB
// - each word has an extra invisible "1" encoded at the end
// - tape is read/written at slow speed only (21.98 ips)
// - a 0 is encoded with a distance between flux reversals of 1/35200 s
//   (giving a maximum density of about 1600 reversals per inch)
// - a 1 is encoded with a distance that's 1.75 times that of a 0
//
// This driver is based on the following model of the actual TACO/tape system:
// * Tape immediately reaches working speed (no spin-up time)
// * Inversion of tape direction and change of speed are immediate as well
// * Time & distance to stop the tape are modeled, though. Firmware is upset by
//   a tape with null braking time/distance.
// * Speed of tape is exceptionally accurate. Real tape was controlled by a closed loop
//   with something like 1% accuracy on speed.
// * Storage is modeled by one "map" data structure per track. Each map maps the tape position
//   to the 16-bit word stored at that position. Gaps are modeled by lack of data in the map.
//   There is no model of the physical encoding of bits (except to compute how long each word
//   is on tape).
// * Read threshold is ignored. Real tapes could be read with either a low or high threshold.
// * "Flag" bit is used as a busy/ready signal in real TACO. Here I assumed the device is
//   always ready, so Flag is always active.
// * I tried to fill the (many) gaps on chip behavior with "sensible" solutions. I could only
//   validate my solutions by running the original firmware in MAME, though (no real hw at hand).
//
// TODOs/issues:
// * Some code cleanup
// * Handling of tape holes seems to be wrong: test "C" of test ROM only works partially
// * Find out what is read from register R6
// * Handle device_image_interface::call_display to show state of tape
// * Find more info on TACO chips (does anyone with a working 9845 or access to internal HP docs want to
//   help me here, please?)
//
#include "emu.h"
#include "hp_taco.h"

// Debugging
#define VERBOSE 1
#define LOG(x)  do { if (VERBOSE) logerror x; } while (0)
#define VERBOSE_0 0
#define LOG_0(x)  do { if (VERBOSE_0) logerror x; } while (0)

// Macros to clear/set single bits
#define BIT_MASK(n) (1U << (n))
#define BIT_CLR(w , n)  ((w) &= ~BIT_MASK(n))
#define BIT_SET(w , n)  ((w) |= BIT_MASK(n))

// Timers
enum {
		TAPE_TMR_ID,
		HOLE_TMR_ID
};

// Constants
#define CMD_REG_MASK    0xffc0  // Command register mask
#define STATUS_REG_MASK 0x003f  // Status register mask
#define TACH_TICKS_PER_INCH     968     // Tachometer pulses per inch of tape movement
#define TAPE_POS_FRACT  1024    // 10 bits of fractional part in tape_pos_t
#define ONE_INCH_POS    (TACH_TICKS_PER_INCH * TAPE_POS_FRACT)  // Value in tape_pos_t representing 1 inch of tape
#define TACH_FREQ_SLOW  21276   // Tachometer pulse frequency for slow speed (21.98 ips)
#define TACH_FREQ_FAST  87196   // Tachometer pulse frequency for fast speed (90.08 ips)
#define TAPE_LENGTH     ((140 * 12 + 72 * 2) * ONE_INCH_POS)    // Tape length: 140 ft of usable tape + 72" of punched tape at either end
#define TAPE_INIT_POS   (80 * ONE_INCH_POS)     // Initial tape position: 80" from beginning (just past the punched part)
#define ZERO_BIT_LEN    619     // Length of 0 bits at slow tape speed: 1/(35200 Hz)
#define ONE_BIT_LEN     1083    // Length of 1 bits at slow tape speed: 1.75 times ZERO_BIT_LEN
#define QUICK_CMD_USEC  25      // usec for "quick" command execution
#define FAST_BRAKE_MSEC 73      // Braking time from fast speed to stop (2 ips) in msec (deceleration is 1200 in/s^2)
#define SLOW_BRAKE_MSEC 17      // Braking time from slow speed to stop in msec
#define FAST_BRAKE_DIST 3350450 // Braking distance at fast speed (~3.38 in)
#define SLOW_BRAKE_DIST 197883  // Braking distance at slow speed (~0.2 in)
#define PREAMBLE_WORD   0       // Value of preamble word
#define END_GAP_LENGTH  (6 * ONE_INCH_POS)      // Length of final gap: 6"
#define MIN_IRG_LENGTH  ((tape_pos_t)(0.2 * ONE_INCH_POS))      // Minimum length of IRGs: 0.2" (from 9825, not sure about value in TACO)
#define NULL_TAPE_POS   ((tape_pos_t)-1)        // Special value for invalid/unknown tape position
#define NO_DATA_GAP     (17 * ONE_BIT_LEN)      // Minimum gap size to detect end of data: length of longest word (0xffff)
#define FILE_MAGIC      0x4f434154      // Magic value at start of image file: "TACO"

// Parts of command register
#define CMD_CODE(reg) \
		(((reg) >> 10) & 0x1f)
#define DIR_FWD(reg) \
		(BIT(reg , 15))
#define SPEED_FAST(reg) \
		(BIT(reg , 7))
#define CMD_OPT(reg) \
		(BIT(reg , 6))
#define UNKNOWN_B9(reg) \
		(BIT(reg , 9))
#define DIR_FWD_MASK    BIT_MASK(15)    // Direction = forward
#define SPEED_FAST_MASK BIT_MASK(7)     // Speed = fast

// Commands
enum {
		CMD_INDTA_INGAP,        // 00: scan for data first then for gap
		CMD_UNK_01,             // 01: unknown
		CMD_FINAL_GAP,          // 02: write final gap
		CMD_INIT_WRITE,         // 03: write words for tape formatting
		CMD_STOP,               // 04: stop
		CMD_UNK_05,             // 05: unknown
		CMD_SET_TRACK,          // 06: set A/B track
		CMD_UNK_07,             // 07: unknown
		CMD_UNK_08,             // 08: unknown
		CMD_UNK_09,             // 09: unknown
		CMD_MOVE,               // 0a: move tape
		CMD_UNK_0b,             // 0b: unknown
		CMD_INGAP_MOVE,         // 0c: scan for gap then move a bit further (used to gain some margin when inverting tape movement)
		CMD_UNK_0d,             // 0d: unknown
		CMD_CLEAR,              // 0e: clear errors/unlatch status bits
		CMD_UNK_0f,             // 0f: unknown
		CMD_NOT_INDTA,          // 10: scan for end of data
		CMD_UNK_11,             // 11: unknown
		CMD_UNK_12,             // 12: unknown
		CMD_UNK_13,             // 13: unknown
		CMD_UNK_14,             // 14: unknown
		CMD_UNK_15,             // 15: unknown
		CMD_WRITE_IRG,          // 16: write inter-record gap
		CMD_UNK_17,             // 17: unknown
		CMD_SCAN_RECORDS,       // 18: scan records (count IRGs)
		CMD_RECORD_WRITE,       // 19: write record words
		CMD_MOVE_INDTA,         // 1a: move then scan for data
		CMD_UNK_1b,             // 1b: unknown (for now it seems harmless to handle it as NOP)
		CMD_DELTA_MOVE_HOLE,    // 1c: move tape a given distance, intr at end or first hole found (whichever comes first)
		CMD_START_READ,         // 1d: start record reading
		CMD_DELTA_MOVE_IRG,     // 1e: move tape a given distance, detect gaps in parallel
		CMD_END_READ            // 1f: stop reading
};

// Bits of status register
#define STATUS_HOLE_BIT         0       // Hole detected
#define STATUS_CART_OUT_BIT     1       // Cartridge out
#define STATUS_SFAIL_BIT        2       // Servo failure
#define STATUS_WPR_BIT          3       // Write protection
#define STATUS_GAP_BIT          4       // Gap detected
#define STATUS_TRACKB_BIT       5       // Track B selected
#define STATUS_CART_OUT_MASK    BIT_MASK(STATUS_CART_OUT_BIT)   // Cartridge out
#define STATUS_WPR_MASK         BIT_MASK(STATUS_WPR_BIT)        // Write protection
#define STATUS_ERR_MASK         (STATUS_CART_OUT_MASK)  // Mask of errors in status reg.

// *** Position of tape holes ***
// At beginning of tape:
// *START*
// |<-----24"----->|<---12"--->|<---12"--->|<-----24"----->|
//               O   O       O   O       O   O             O
//               |<->|       |<->|       |<->|
//               0.218"      0.218"      0.218"
// At end of tape:
//                                                     *END*
// |<-----24"----->|<---12"--->|<---12"--->|<-----24"----->|
// O               O           O           O
//
static const hp_taco_device::tape_pos_t tape_holes[] = {
		(hp_taco_device::tape_pos_t)(23.891 * ONE_INCH_POS),    // 24 - 0.218 / 2
		(hp_taco_device::tape_pos_t)(24.109 * ONE_INCH_POS),    // 24 + 0.218 / 2
		(hp_taco_device::tape_pos_t)(35.891 * ONE_INCH_POS),    // 36 - 0.218 / 2
		(hp_taco_device::tape_pos_t)(36.109 * ONE_INCH_POS),    // 36 + 0.218 / 2
		(hp_taco_device::tape_pos_t)(47.891 * ONE_INCH_POS),    // 48 - 0.218 / 2
		(hp_taco_device::tape_pos_t)(48.109 * ONE_INCH_POS),    // 48 + 0.218 / 2
		72 * ONE_INCH_POS,      // 72
		1752 * ONE_INCH_POS,    // 1752
		1776 * ONE_INCH_POS,    // 1776
		1788 * ONE_INCH_POS,    // 1788
		1800 * ONE_INCH_POS     // 1800
};

// Device type definition
const device_type HP_TACO = &device_creator<hp_taco_device>;

// Constructors
hp_taco_device::hp_taco_device(const machine_config &mconfig, device_type type, const char *name, const char *tag, device_t *owner, UINT32 clock, const char *shortname)
		: device_t(mconfig, type, name, tag, owner, clock, shortname, __FILE__),
			device_image_interface(mconfig , *this),
			m_irq_handler(*this),
			m_flg_handler(*this),
			m_sts_handler(*this),
			m_tape_pos(TAPE_INIT_POS),
			m_image_dirty(false)
{
		clear_state();
}

hp_taco_device::hp_taco_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock)
		: device_t(mconfig, HP_TACO, "HP TACO", tag, owner, clock, "TACO", __FILE__),
			device_image_interface(mconfig , *this),
			m_irq_handler(*this),
			m_flg_handler(*this),
			m_sts_handler(*this),
			m_tape_pos(TAPE_INIT_POS),
			m_image_dirty(false)
{
		clear_state();
}

WRITE16_MEMBER(hp_taco_device::reg_w)
{
		LOG_0(("wr R%u = %04x\n", 4 + offset , data));

		// Any I/O activity clears IRQ
		irq_w(false);

		switch (offset) {
		case 0:
				// Data register
				m_data_reg = data;
				m_data_reg_full = true;
				break;

		case 1:
				// Command register
				start_cmd_exec(data & CMD_REG_MASK);
				break;

		case 2:
				// Tachometer register
				m_tach_reg = data;
				break;

		case 3:
				// Timing register
				m_timing_reg = data;
				break;
		}
}

READ16_MEMBER(hp_taco_device::reg_r)
{
		UINT16 res = 0;

		// Any I/O activity clears IRQ
		irq_w(false);

		switch (offset) {
		case 0:
				// Data register
				res = m_data_reg;
				break;

		case 1:
				// Command & status register
				res = (m_cmd_reg & CMD_REG_MASK) | (m_status_reg & STATUS_REG_MASK);
				break;

		case 2:
				// Tachometer register
				res = m_tach_reg;
				break;

		case 3:
				// Checksum register: it clears when read
				res = m_checksum_reg;
				m_checksum_reg = 0;
				break;
		}

		LOG_0(("rd R%u = %04x\n", 4 + offset , res));

		return res;
}

READ_LINE_MEMBER(hp_taco_device::flg_r)
{
		return m_flg;
}

READ_LINE_MEMBER(hp_taco_device::sts_r)
{
		return m_sts;
}

// device_config_complete
void hp_taco_device::device_config_complete()
{
		LOG(("device_config_complete"));
	update_names();
}

// device_start
void hp_taco_device::device_start()
{
		LOG(("device_start"));
		m_irq_handler.resolve_safe();
		m_flg_handler.resolve_safe();
		m_sts_handler.resolve_safe();

		save_item(NAME(m_data_reg));
		save_item(NAME(m_data_reg_full));
		save_item(NAME(m_cmd_reg));
		save_item(NAME(m_cmd_state));
		save_item(NAME(m_status_reg));
		save_item(NAME(m_tach_reg));
		save_item(NAME(m_checksum_reg));
		save_item(NAME(m_timing_reg));
		save_item(NAME(m_irq));
		save_item(NAME(m_flg));
		save_item(NAME(m_sts));
		save_item(NAME(m_tape_pos));
		save_item(NAME(m_start_time));
		save_item(NAME(m_tape_fwd));
		save_item(NAME(m_tape_fast));
		save_item(NAME(m_image_dirty));
		save_item(NAME(m_tape_wr));
		save_item(NAME(m_rw_pos));
		save_item(NAME(m_next_word));
		save_item(NAME(m_rd_it_valid));
		save_item(NAME(m_gap_detect_start));

		m_tape_timer = timer_alloc(TAPE_TMR_ID);
		m_hole_timer = timer_alloc(HOLE_TMR_ID);
}

// device_stop
void hp_taco_device::device_stop()
{
}

// device_reset
void hp_taco_device::device_reset()
{
		LOG(("device_reset"));
		clear_state();

		m_irq = false;
		m_flg = true;

		m_irq_handler(false);
		m_flg_handler(true);
		set_error(false);
}

void hp_taco_device::device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr)
{
		if (CMD_CODE(m_cmd_reg) != CMD_STOP) {
				update_tape_pos();
		}

		switch (id) {
		case TAPE_TMR_ID:
				LOG_0(("Tape tmr @%g\n" , machine().time().as_double()));

				tape_pos_t length;

				switch (CMD_CODE(m_cmd_reg)) {
				case CMD_INDTA_INGAP:
						if (m_cmd_state == 0) {
								m_cmd_state = 1;
								tape_pos_t target = m_tape_pos;
								if (next_n_gap(target, 1, MIN_IRG_LENGTH)) {
										m_tape_timer->adjust(time_to_target(target));
								}
								return;
						}
						break;

				case CMD_RECORD_WRITE:
						if (m_cmd_state == 0) {
								if (m_rd_it->second == PREAMBLE_WORD) {
										LOG_0(("Got preamble\n"));
										m_cmd_state = 1;
										// m_rw_pos already at correct position
										m_tape_timer->adjust(fetch_next_wr_word());
										break;
								} else {
										adv_res_t res = adv_it(m_rd_it);
										if (res != ADV_NO_MORE_DATA) {
												m_tape_timer->adjust(time_to_rd_next_word(m_rw_pos));
										}
										// No IRQ
										return;
								}
						}
						// Intentional fall-through
				case CMD_INIT_WRITE:
						write_word(m_rw_pos , m_next_word , length);
						pos_offset(m_rw_pos , length);
						// Just to be sure..
						m_tape_pos = m_rw_pos;
						m_tape_timer->adjust(fetch_next_wr_word());
						break;

				case CMD_STOP:
						move_tape_pos(m_tape_fast ? FAST_BRAKE_DIST : SLOW_BRAKE_DIST);
						stop_tape();
						break;

				case CMD_INGAP_MOVE:
						if (m_cmd_state == 0) {
								m_cmd_state = 1;
								m_tape_timer->adjust(time_to_tach_pulses());
								return;
						}
						break;

				case CMD_FINAL_GAP:
				case CMD_WRITE_IRG:
						write_gap(m_rw_pos , m_tape_pos);
						m_hole_timer->reset();
						break;

				case CMD_SCAN_RECORDS:
						if (m_cmd_state == 0) {
								m_cmd_state = 1;
								tape_pos_t target = m_tape_pos;
								if (next_n_gap(target, 0x10000U - m_tach_reg, MIN_IRG_LENGTH)) {
										LOG_0(("%u gaps @%d\n" , 0x10000U - m_tach_reg, target));
										m_tape_timer->adjust(time_to_target(target));
								}
								return;
						} else {
								m_hole_timer->reset();
						}
						break;

				case CMD_MOVE_INDTA:
						if (m_cmd_state == 0) {
								if (next_data(m_rd_it , m_tape_pos , true)) {
										m_cmd_state = 1;
										m_tape_timer->adjust(time_to_target(farthest_end(m_rd_it)));
								}
								// No IRQ
								return;
						}
						// m_cmd_state == 1 -> IRQ & cmd end
						break;

				case CMD_DELTA_MOVE_HOLE:
				case CMD_DELTA_MOVE_IRG:
						// Interrupt at end of movement
						m_hole_timer->reset();
						break;

				case CMD_START_READ:
						{
								bool set_intr = true;
								// Just to be sure..
								m_tape_pos = m_rw_pos;
								if (m_cmd_state == 0) {
										set_intr = false;
										if (m_rd_it->second == PREAMBLE_WORD) {
												m_cmd_state = 1;
										}
										LOG_0(("Got preamble\n"));
								} else {
										m_data_reg = m_rd_it->second;
										m_checksum_reg += m_data_reg;
										LOG_0(("RD %04x\n" , m_data_reg));
								}
								adv_res_t res = adv_it(m_rd_it);
								LOG_0(("adv_it %d\n" , res));
								if (res == ADV_NO_MORE_DATA) {
										m_rd_it_valid = false;
								} else {
										if (res == ADV_DISCONT_DATA) {
												// Hit a gap, restart preamble search
												m_cmd_state = 0;
										}
										m_tape_timer->adjust(time_to_rd_next_word(m_rw_pos));
								}
								if (!set_intr) {
										return;
								}
						}
						break;

				case CMD_END_READ:
						{
								m_tape_pos = m_rw_pos;
								// Note: checksum is not updated
								m_data_reg = m_rd_it->second;
								LOG_0(("Final RD %04x\n" , m_data_reg));
								adv_res_t res = adv_it(m_rd_it);
								if (res == ADV_NO_MORE_DATA) {
										m_rd_it_valid = false;
								}
								m_hole_timer->reset();
						}
						break;

				default:
						// Other commands: just raise irq
						break;
				}
				irq_w(true);
				break;

		case HOLE_TMR_ID:
				LOG_0(("Hole tmr @%g\n" , machine().time().as_double()));

				BIT_SET(m_status_reg , STATUS_HOLE_BIT);

				switch (CMD_CODE(m_cmd_reg)) {
				case CMD_FINAL_GAP:
				case CMD_WRITE_IRG:
						write_gap(m_rw_pos , m_tape_pos);
						m_rw_pos = m_tape_pos;
						break;

				case CMD_SCAN_RECORDS:
				case CMD_DELTA_MOVE_HOLE:
						// Cmds 18 & 1c are terminated at first hole
						m_tape_timer->reset();
						irq_w(true);
						// No reloading of hole timer
						return;

				case CMD_DELTA_MOVE_IRG:
						// TODO: update r6
						m_hole_timer->adjust(time_to_next_hole());
						// No IRQ at holes
						return;

				case CMD_START_READ:
				case CMD_END_READ:
						set_error(true);
						break;

				default:
						// Other cmds: default processing (update tape pos, set IRQ, schedule timer for next hole)
						break;
				}

				irq_w(true);
				m_hole_timer->adjust(time_to_next_hole());
				break;

		default:
				break;
		}
}

void hp_taco_device::clear_state(void)
{
		m_data_reg = 0;
		m_data_reg_full = false;
		m_cmd_reg = 0;
		m_status_reg = 0;
		m_tach_reg = 0;
		m_checksum_reg = 0;
		m_timing_reg = 0;
		m_cmd_state = 0;
		// m_tape_pos is not reset, tape stays where it is
		m_start_time = attotime::never;
		m_tape_fwd = false;
		m_tape_fast = false;
		// m_image_dirty is not touched
		m_tape_wr = false;
		m_rw_pos = 0;
		m_next_word = 0;
		m_rd_it_valid = false;
		m_gap_detect_start = NULL_TAPE_POS;

		set_tape_present(false);
		set_tape_present(is_loaded());
}

void hp_taco_device::irq_w(bool state)
{
		if (state != m_irq) {
				m_irq = state;
				m_irq_handler(state);
				LOG_0(("IRQ = %d\n" , state));
		}
}

void hp_taco_device::set_error(bool state)
{
		m_sts = !state;
		m_sts_handler(m_sts);
		LOG_0(("error = %d\n" , state));
}

unsigned hp_taco_device::speed_to_tick_freq(void) const
{
		return m_tape_fast ? TACH_FREQ_FAST * TAPE_POS_FRACT : TACH_FREQ_SLOW * TAPE_POS_FRACT;
}

bool hp_taco_device::pos_offset(tape_pos_t& pos , tape_pos_t offset) const
{
		if (offset == 0) {
				return true;
		}

		if (!m_tape_fwd) {
				offset = -offset;
		}

		pos += offset;

		// In real life tape would unspool..
		if (pos > TAPE_LENGTH) {
				pos = TAPE_LENGTH;
				return false;
		} else if (pos < 0) {
				pos = 0;
				return false;
		} else {
				return true;
		}
}

void hp_taco_device::move_tape_pos(tape_pos_t delta_pos)
{
		tape_pos_t tape_start_pos = m_tape_pos;
		if (!pos_offset(m_tape_pos , delta_pos)) {
				LOG(("Tape unspooled!\n"));
		}
		m_start_time = machine().time();
		LOG_0(("Tape pos = %u\n" , m_tape_pos));
		if (any_hole(tape_start_pos , m_tape_pos)) {
				// Crossed one or more holes
				BIT_SET(m_status_reg , STATUS_HOLE_BIT);
		}
}

void hp_taco_device::update_tape_pos(void)
{
		if (m_start_time.is_never()) {
				// Tape not moving
				return;
		}

		attotime delta_time(machine().time() - m_start_time);
		LOG_0(("delta_time = %g\n" , delta_time.as_double()));
		// How many tachometer ticks has the tape moved?
		tape_pos_t delta_tach = (tape_pos_t)(delta_time.as_ticks(speed_to_tick_freq()));
		LOG_0(("delta_tach = %u\n" , delta_tach));

		move_tape_pos(delta_tach);

		// Gap detection
		bool gap_detected = false;
		if (m_gap_detect_start != NULL_TAPE_POS && abs(m_gap_detect_start - m_tape_pos) >= MIN_IRG_LENGTH) {
				tape_pos_t tmp = m_tape_pos;
				pos_offset(tmp , -MIN_IRG_LENGTH);
				gap_detected = just_gap(tmp , m_tape_pos);
		}
		if (gap_detected) {
				BIT_SET(m_status_reg, STATUS_GAP_BIT);
		} else {
				BIT_CLR(m_status_reg, STATUS_GAP_BIT);
		}
}

void hp_taco_device::ensure_a_lt_b(tape_pos_t& a , tape_pos_t& b)
{
		if (a > b) {
				// Ensure A always comes before B
				tape_pos_t tmp;
				tmp = a;
				a = b;
				b = tmp;
		}
}

// Is there any hole in a given section of tape?
bool hp_taco_device::any_hole(tape_pos_t tape_pos_a , tape_pos_t tape_pos_b)
{
		ensure_a_lt_b(tape_pos_a , tape_pos_b);

		for (tape_pos_t hole : tape_holes) {
				if (tape_pos_a < hole && tape_pos_b >= hole) {
						return true;
				}
		}

		return false;
}

// Position of next hole tape will reach in a given direction
hp_taco_device::tape_pos_t hp_taco_device::next_hole(void) const
{
		if (m_tape_fwd) {
				for (tape_pos_t hole : tape_holes) {
						if (hole > m_tape_pos) {
								LOG_0(("next hole fwd @%u = %u\n" , m_tape_pos , hole));
								return hole;
						}
				}
				// No more holes: will hit end of tape
				return TAPE_LENGTH;
		} else {
				for (int i = (sizeof(tape_holes) / sizeof(tape_holes[ 0 ])) - 1; i >= 0; i--) {
						if (tape_holes[ i ] < m_tape_pos) {
								LOG_0(("next hole rev @%u = %u\n" , m_tape_pos , tape_holes[ i ]));
								return tape_holes[ i ];
						}
				}
				// No more holes: will hit start of tape
				return 0;
		}
}

attotime hp_taco_device::time_to_distance(tape_pos_t distance) const
{
		// +1 for rounding
		return attotime::from_ticks(distance + 1 , speed_to_tick_freq());
}

attotime hp_taco_device::time_to_target(tape_pos_t target) const
{
		return time_to_distance(abs(target - m_tape_pos));
}

bool hp_taco_device::start_tape_cmd(UINT16 cmd_reg , UINT16 must_be_1 , UINT16 must_be_0)
{
		m_cmd_reg = cmd_reg;

		UINT16 to_be_tested = (m_cmd_reg & CMD_REG_MASK) | (m_status_reg & STATUS_REG_MASK);
		// Bits in STATUS_ERR_MASK must always be 0
		must_be_0 |= STATUS_ERR_MASK;

		// It's not an error if the error state is already set (sts false)
		if (((to_be_tested & (must_be_1 | must_be_0)) ^ must_be_1) != 0) {
				set_error(true);
				return false;
		} else {
				bool prev_tape_wr = m_tape_wr;
				bool prev_tape_fwd = m_tape_fwd;
				bool prev_tape_fast = m_tape_fast;
				bool not_moving = m_start_time.is_never();

				m_start_time = machine().time();
				m_tape_wr = (must_be_0 & STATUS_WPR_MASK) != 0;
				m_tape_fwd = DIR_FWD(m_cmd_reg);
				m_tape_fast = SPEED_FAST(m_cmd_reg);
				// TODO: remove?
				BIT_CLR(m_status_reg, STATUS_HOLE_BIT);

				if (m_tape_wr) {
						// Write command: disable gap detector
						m_gap_detect_start = NULL_TAPE_POS;
						BIT_CLR(m_status_reg, STATUS_GAP_BIT);
						m_image_dirty = true;
				} else if (not_moving || prev_tape_wr != m_tape_wr || prev_tape_fwd != m_tape_fwd || prev_tape_fast != m_tape_fast) {
						// Tape started right now, switched from writing to reading, direction changed or speed changed: (re)start gap detector
						m_gap_detect_start = m_tape_pos;
						BIT_CLR(m_status_reg, STATUS_GAP_BIT);
				}
				return true;
		}
}

void hp_taco_device::stop_tape(void)
{
		m_start_time = attotime::never;
		m_gap_detect_start = NULL_TAPE_POS;
}

hp_taco_device::tape_track_t& hp_taco_device::current_track(void)
{
		return m_tracks[ BIT(m_status_reg , STATUS_TRACKB_BIT) ];
}

// Return physical length of a 16-bit word on tape
hp_taco_device::tape_pos_t hp_taco_device::word_length(tape_word_t w)
{
		unsigned zeros , ones;

		// pop count of w
		ones = (w & 0x5555) + ((w >> 1) & 0x5555);
		ones = (ones & 0x3333) + ((ones >> 2) & 0x3333);
		ones = (ones & 0x0f0f) + ((ones >> 4) & 0x0f0f);
		ones = (ones & 0x00ff) + ((ones >> 8) & 0x00ff);

		zeros = 16 - ones;

		return zeros * ZERO_BIT_LEN + (ones + 1) * ONE_BIT_LEN;
}

hp_taco_device::tape_pos_t hp_taco_device::word_end_pos(const tape_track_t::iterator& it)
{
	return it->first + word_length(it->second);
}

void hp_taco_device::adjust_it(tape_track_t& track , tape_track_t::iterator& it , tape_pos_t pos)
{
		if (it != track.begin()) {
				it--;
				if (word_end_pos(it) <= pos) {
						it++;
				}
		}
}

// Write a word on current tape track
void hp_taco_device::write_word(tape_pos_t start , tape_word_t word , tape_pos_t& length)
{
		tape_track_t& track = current_track();
		tape_track_t::iterator it_low = track.lower_bound(start);
		adjust_it(track , it_low , start);
		length = word_length(word);
		tape_pos_t end_pos = start + length;
		tape_track_t::iterator it_high = track.lower_bound(end_pos);

		track.erase(it_low , it_high);

		track.insert(it_high , std::make_pair(start, word));
		LOG_0(("WR %04x @ T%u:%u\n" , word , BIT(m_status_reg , STATUS_TRACKB_BIT) , start));
}

// Write a gap on current track
void hp_taco_device::write_gap(tape_pos_t a , tape_pos_t b)
{
		ensure_a_lt_b(a , b);
		tape_track_t& track = current_track();
		tape_track_t::iterator it_low = track.lower_bound(a);
		adjust_it(track , it_low , a);
		tape_track_t::iterator it_high = track.lower_bound(b);

		track.erase(it_low, it_high);

		LOG_0(("GAP on T%u:[%u,%u)\n" , BIT(m_status_reg , STATUS_TRACKB_BIT) , a , b));
}

bool hp_taco_device::just_gap(tape_pos_t a , tape_pos_t b)
{
		ensure_a_lt_b(a , b);
		tape_track_t& track = current_track();
		tape_track_t::iterator it_low = track.lower_bound(a);
		tape_track_t::iterator it_high = track.lower_bound(b);

		adjust_it(track, it_low, a);

		return it_low == it_high;
}

hp_taco_device::tape_pos_t hp_taco_device::farthest_end(const tape_track_t::iterator& it) const
{
		if (m_tape_fwd) {
				return word_end_pos(it);
		} else {
				return it->first;
		}
}

bool hp_taco_device::next_data(tape_track_t::iterator& it , tape_pos_t pos , bool inclusive)
{
		tape_track_t& track = current_track();
		it = track.lower_bound(pos);
		if (m_tape_fwd) {
				if (inclusive) {
						adjust_it(track, it, pos);
				}
				return it != track.end();
		} else {
				// Never more than 2 iterations
				do {
						if (it == track.begin()) {
								it = track.end();
								return false;
						}
						it--;
				} while (!inclusive && word_end_pos(it) > pos);
				return true;
		}
}

hp_taco_device::adv_res_t hp_taco_device::adv_it(tape_track_t::iterator& it)
{
		tape_track_t& track = current_track();
		if (m_tape_fwd) {
				tape_pos_t prev_pos = word_end_pos(it);
				it++;
				if (it == track.end()) {
						return ADV_NO_MORE_DATA;
				} else {
						adv_res_t res = prev_pos == it->first ? ADV_CONT_DATA : ADV_DISCONT_DATA;
						return res;
				}
		} else {
				if (it == track.begin()) {
						it = track.end();
						return ADV_NO_MORE_DATA;
				} else {
						tape_pos_t prev_pos = it->first;
						it--;
						return prev_pos == word_end_pos(it) ? ADV_CONT_DATA : ADV_DISCONT_DATA;
				}
		}
}

attotime hp_taco_device::fetch_next_wr_word(void)
{
		if (m_data_reg_full) {
				m_next_word = m_data_reg;
				m_data_reg_full = false;
				LOG_0(("next %04x (DR)\n" , m_next_word));
		} else {
				// When data register is empty, write checksum word
				m_next_word = m_checksum_reg;
				LOG_0(("next %04x (CS)\n" , m_next_word));
		}
		// Update checksum with new word
		m_checksum_reg += m_next_word;

		return time_to_distance(word_length(m_next_word));
}

attotime hp_taco_device::time_to_rd_next_word(tape_pos_t& word_rd_pos)
{
		if (m_rd_it_valid) {
				word_rd_pos = farthest_end(m_rd_it);
				return time_to_target(word_rd_pos);
		} else {
				return attotime::never;
		}
}

/**
 * Scan for next "n_gaps" gaps
 *
 * @param[in,out] pos Start position on input, start of gap on output
 * @param it Pointer to data word where scan is to start
 * @param n_gaps Number of gaps to scan
 * @param min_gap Minimum gap size
 *
 * @return true if n_gaps gaps are found
 */
bool hp_taco_device::next_n_gap(tape_pos_t& pos , tape_track_t::iterator it , unsigned n_gaps , tape_pos_t min_gap)
{
		tape_track_t& track = current_track();
		bool done = false;
		tape_track_t::iterator prev_it;

		if (m_tape_fwd) {
				tape_pos_t next_pos;

				while (1) {
						if (it == track.end()) {
								next_pos = TAPE_LENGTH;
								done = true;
						} else {
								next_pos = it->first;
						}
						if (((next_pos - pos) >= min_gap && --n_gaps == 0) || done) {
								break;
						}
						adv_res_t adv_res;
						do {
								prev_it = it;
								adv_res = adv_it(it);
						} while (adv_res == ADV_CONT_DATA);
						pos = word_end_pos(prev_it);
				}
		} else {
				tape_pos_t next_pos;

				while (1) {
						if (it == track.end()) {
								next_pos = 0;
								done = true;
						} else {
								next_pos = word_end_pos(it);
						}
						if (((pos - next_pos) >= min_gap && --n_gaps == 0) || done) {
								break;
						}
						adv_res_t adv_res;
						do {
								prev_it = it;
								adv_res = adv_it(it);
						} while (adv_res == ADV_CONT_DATA);
						pos = prev_it->first;
				}
		}

		// Set "pos" where minimum gap size is met
		pos_offset(pos , min_gap);

		return n_gaps == 0;
}

bool hp_taco_device::next_n_gap(tape_pos_t& pos , unsigned n_gaps , tape_pos_t min_gap)
{
		tape_track_t::iterator it;
		// First align with next data
		next_data(it, pos, true);
		// Then scan for n_gaps
		return next_n_gap(pos, it, n_gaps, min_gap);
}

void hp_taco_device::clear_tape(void)
{
		for (unsigned track_n = 0; track_n < 2; track_n++) {
				m_tracks[ track_n ].clear();
		}
}

void hp_taco_device::dump_sequence(tape_track_t::const_iterator it_start , unsigned n_words)
{
		if (n_words) {
				UINT32 tmp32;
				UINT16 tmp16;

				tmp32 = n_words;
				fwrite(&tmp32 , sizeof(tmp32));
				tmp32 = it_start->first;
				fwrite(&tmp32 , sizeof(tmp32));

				for (unsigned i = 0; i < n_words; i++) {
						tmp16 = it_start->second;
						fwrite(&tmp16 , sizeof(tmp16));
						it_start++;
				}
		}
}

void hp_taco_device::save_tape(void)
{
		UINT32 tmp32;

		fseek(0, SEEK_SET);

		tmp32 = FILE_MAGIC;
		fwrite(&tmp32 , sizeof(tmp32));

		for (unsigned track_n = 0; track_n < 2; track_n++) {
				const tape_track_t& track = m_tracks[ track_n ];
				tape_pos_t next_pos = (tape_pos_t)-1;
				unsigned n_words = 0;
				tape_track_t::const_iterator it_start;
				for (tape_track_t::const_iterator it = track.cbegin(); it != track.cend(); it++) {
						if (it->first != next_pos) {
								dump_sequence(it_start , n_words);
								it_start = it;
								n_words = 0;
						}
						next_pos = it->first + word_length(it->second);
						n_words++;
				}
				dump_sequence(it_start , n_words);
				// End of track
				tmp32 = (UINT32)-1;
				fwrite(&tmp32 , sizeof(tmp32));
		}
}

bool hp_taco_device::load_track(tape_track_t& track)
{
		UINT32 tmp32;

		track.clear();

		while (1) {
				if (fread(&tmp32 , sizeof(tmp32)) != sizeof(tmp32)) {
						return false;
				}

				if (tmp32 == (UINT32)-1) {
						return true;
				}

				unsigned n_words = tmp32;

				if (fread(&tmp32 , sizeof(tmp32)) != sizeof(tmp32)) {
						return false;
				}

				tape_pos_t pos = (tape_pos_t)tmp32;

				for (unsigned i = 0; i < n_words; i++) {
						UINT16 tmp16;

						if (fread(&tmp16 , sizeof(tmp16)) != sizeof(tmp16)) {
								return false;
						}

						track.insert(std::make_pair(pos , tmp16));
						pos += word_length(tmp16);
				}
		}
}

bool hp_taco_device::load_tape(void)
{
		UINT32 magic;

		if (fread(&magic , sizeof(magic)) != sizeof(magic) ||
			magic != FILE_MAGIC) {
				return false;
		}

		for (unsigned track_n = 0; track_n < 2; track_n++) {
				if (!load_track(m_tracks[ track_n ])) {
						LOG(("load_tape failed"));
						clear_tape();
						return false;
				}
		}

		LOG(("load_tape done\n"));
		return true;
}

void hp_taco_device::set_tape_present(bool present)
{
		if (present) {
				if (is_readonly()) {
						BIT_SET(m_status_reg, STATUS_WPR_BIT);
				} else {
						BIT_CLR(m_status_reg, STATUS_WPR_BIT);
				}
				// STATUS_CART_OUT_BIT is reset by CMD_CLEAR
		} else {
				BIT_SET(m_status_reg, STATUS_CART_OUT_BIT);
				BIT_SET(m_status_reg, STATUS_WPR_BIT);
		}
}

attotime hp_taco_device::time_to_next_hole(void) const
{
		return time_to_target(next_hole());
}

attotime hp_taco_device::time_to_tach_pulses(void) const
{
		return time_to_distance((tape_pos_t)(0x10000U - m_tach_reg) * TAPE_POS_FRACT);
}

void hp_taco_device::start_cmd_exec(UINT16 new_cmd_reg)
{
		LOG(("Cmd = %02x\n" , CMD_CODE(new_cmd_reg)));

		update_tape_pos();

		attotime cmd_duration = attotime::never;
		attotime time_to_hole = attotime::never;

		unsigned new_cmd_code = CMD_CODE(new_cmd_reg);

		if (new_cmd_code != CMD_START_READ &&
			new_cmd_code != CMD_END_READ &&
			new_cmd_code != CMD_CLEAR) {
				m_rd_it_valid = false;
		}

		switch (new_cmd_code) {
		case CMD_INDTA_INGAP:
				// Errors: CART OUT,FAST SPEED
				if (start_tape_cmd(new_cmd_reg , 0 , SPEED_FAST_MASK)) {
						m_cmd_state = 0;
						if (next_data(m_rd_it , m_tape_pos , true)) {
								cmd_duration = time_to_target(farthest_end(m_rd_it));
						}
				}
				break;

		case CMD_FINAL_GAP:
				// Errors: WP,CART OUT
				if (start_tape_cmd(new_cmd_reg , 0 , STATUS_WPR_MASK)) {
						m_rw_pos = m_tape_pos;
						cmd_duration = time_to_distance(END_GAP_LENGTH);
						time_to_hole = time_to_next_hole();
				}
				break;

		case CMD_CLEAR:
				set_error(false);
				BIT_CLR(m_status_reg, STATUS_HOLE_BIT);
				BIT_CLR(m_status_reg, STATUS_CART_OUT_BIT);
				BIT_CLR(m_status_reg, STATUS_WPR_BIT);
				set_tape_present(is_loaded());
				// This is a special command: it doesn't raise IRQ at completion and it
				// doesn't replace current command
				return;

		case CMD_NOT_INDTA:
				// Errors: CART OUT,FAST SPEED
				if (start_tape_cmd(new_cmd_reg , 0 , SPEED_FAST_MASK)) {
						tape_pos_t target = m_tape_pos;
						if (next_n_gap(target, 1, NO_DATA_GAP)) {
								LOG_0(("End of data @%d\n" , target));
								cmd_duration = time_to_target(target);
						}
						// Holes detected?
				}
				break;

		case CMD_INIT_WRITE:
				// Errors: WP,CART OUT,fast speed,reverse
				if (start_tape_cmd(new_cmd_reg , DIR_FWD_MASK , STATUS_WPR_MASK | SPEED_FAST_MASK)) {
						m_next_word = PREAMBLE_WORD;
						m_rw_pos = m_tape_pos;
						cmd_duration = time_to_distance(word_length(m_next_word));
				}
				break;

		case CMD_STOP:
				if (CMD_CODE(m_cmd_reg) != CMD_STOP) {
						if (m_start_time.is_never()) {
								// Tape is already stopped
								cmd_duration = attotime::from_usec(QUICK_CMD_USEC);
						} else {
								// Start braking timer
								cmd_duration = attotime::from_msec(m_tape_fast ? FAST_BRAKE_MSEC : SLOW_BRAKE_MSEC);
						}
						m_cmd_reg = new_cmd_reg;
				} else {
						// TODO: check if ok
						return;
				}
				break;

		case CMD_SET_TRACK:
				// Don't know if this command really starts the tape or not (probably it doesn't)
				if (start_tape_cmd(new_cmd_reg , 0 , 0)) {
						// When b9 is 0, set track A/B
						// When b9 is 1, ignore command (in TACO chip it has an unknown purpose)
						if (!UNKNOWN_B9(new_cmd_reg)) {
								if (CMD_OPT(new_cmd_reg)) {
										BIT_SET(m_status_reg, STATUS_TRACKB_BIT);
								} else {
										BIT_CLR(m_status_reg, STATUS_TRACKB_BIT);
								}
						}
						cmd_duration = attotime::from_usec(QUICK_CMD_USEC);
				}
				break;

		case CMD_MOVE:
				if (start_tape_cmd(new_cmd_reg , 0 , 0)) {
						time_to_hole = time_to_next_hole();
				}
				break;

		case CMD_INGAP_MOVE:
				// Errors: CART OUT,FAST SPEED
				if (start_tape_cmd(new_cmd_reg , 0 , SPEED_FAST_MASK)) {
						m_cmd_state = 0;
						tape_pos_t target = m_tape_pos;
						if (next_n_gap(target, 1, MIN_IRG_LENGTH)) {
								LOG_0(("IRG @%d\n" , target));
								cmd_duration = time_to_target(target);
						}
						// Holes detected?
				}
				break;

		case CMD_WRITE_IRG:
				// Errors: WP,CART OUT
				if (start_tape_cmd(new_cmd_reg , 0 , STATUS_WPR_MASK)) {
						m_rw_pos = m_tape_pos;
						cmd_duration = time_to_tach_pulses();
						time_to_hole = time_to_next_hole();
				}
				break;

		case CMD_SCAN_RECORDS:
				// Errors: CART OUT
				if (start_tape_cmd(new_cmd_reg , 0 , 0)) {
						m_cmd_state = 0;
						if (next_data(m_rd_it , m_tape_pos , true)) {
								cmd_duration = time_to_target(farthest_end(m_rd_it));
						}
						time_to_hole = time_to_next_hole();
				}
				break;

		case CMD_RECORD_WRITE:
				// Errors: WP,CART OUT,fast speed,reverse
				if (start_tape_cmd(new_cmd_reg , DIR_FWD_MASK , STATUS_WPR_MASK | SPEED_FAST_MASK)) {
						// Search for preamble first
						m_cmd_state = 0;
						m_rd_it_valid = next_data(m_rd_it , m_tape_pos , false);
						cmd_duration = time_to_rd_next_word(m_rw_pos);
						// Holes detected?
				}
				break;

		case CMD_MOVE_INDTA:
				// Errors: CART OUT,FAST SPEED
				if (start_tape_cmd(new_cmd_reg , 0 , SPEED_FAST_MASK)) {
						m_cmd_state = 0;
						cmd_duration = time_to_tach_pulses();
						// Holes detected?
				}
				break;

		case CMD_UNK_1b:
				if (start_tape_cmd(new_cmd_reg , 0 , 0)) {
						// Unknown purpose, but make it a NOP (it's used in "T" test of test ROM)
						cmd_duration = attotime::from_usec(QUICK_CMD_USEC);
				}
				break;

		case CMD_DELTA_MOVE_HOLE:
		case CMD_DELTA_MOVE_IRG:
				if (start_tape_cmd(new_cmd_reg , 0 , 0)) {
						cmd_duration = time_to_tach_pulses();
						time_to_hole = time_to_next_hole();
				}
				break;

		case CMD_START_READ:
				// Yes, you can read tape backwards: test "C" does that!
				// Because of this DIR_FWD_MASK is not in the "must be 1" mask.
				if (start_tape_cmd(new_cmd_reg , 0 , SPEED_FAST_MASK)) {
						// TODO: check anche m_rw_pos sforato
						if (!m_rd_it_valid) {
								// Search for preamble first
								m_cmd_state = 0;
								m_rd_it_valid = next_data(m_rd_it , m_tape_pos , false);
						}

						cmd_duration = time_to_rd_next_word(m_rw_pos);
						time_to_hole = time_to_next_hole();
				}
				break;

		case CMD_END_READ:
				// This command only makes sense after CMD_START_READ
				if (CMD_CODE(m_cmd_reg) == CMD_START_READ && m_cmd_state == 1 &&
					start_tape_cmd(new_cmd_reg , 0 , SPEED_FAST_MASK)) {
						LOG_0(("END_READ %d\n" , m_rd_it_valid));
						cmd_duration = time_to_rd_next_word(m_rw_pos);
						time_to_hole = time_to_next_hole();
				}
				break;

		default:
				LOG(("Unrecognized command\n"));
				break;
		}

		m_tape_timer->adjust(cmd_duration);
		m_hole_timer->adjust(time_to_hole);
}

bool hp_taco_device::call_load()
{
		LOG(("call_load %d\n" , has_been_created()));
		if (has_been_created()) {
				clear_tape();
				save_tape();
		} else if (!load_tape()) {
				seterror(IMAGE_ERROR_INVALIDIMAGE , "Wrong format");
				set_tape_present(false);
				return IMAGE_INIT_FAIL;
		}

		m_image_dirty = false;

		set_tape_present(true);
	return IMAGE_INIT_PASS;
}

bool hp_taco_device::call_create(int format_type, option_resolution *format_options)
{
		LOG(("call_create %d\n" , has_been_created()));
		return call_load();
}

void hp_taco_device::call_unload()
{
		LOG(("call_unload dirty=%d\n" , m_image_dirty));
		if (m_image_dirty) {
				save_tape();
				m_image_dirty = false;
		}

		clear_tape();
		set_tape_present(false);
}

const char *hp_taco_device::file_extensions() const
{
		return "hti";
}
