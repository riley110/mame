#ifndef __BW12__
#define __BW12__

#define SCREEN_TAG			"screen"
#define Z80_TAG				"ic35"
#define MC6845_TAG			"ic14"
#define UPD765_TAG			"ic45"
#define Z80SIO_TAG			"ic15"
#define PIT8253_TAG			"ic34"
#define PIA6821_TAG			"ic16"
#define MC1408_TAG			"ic4"
#define AY3600_TAG			"ic74"
#define CENTRONICS_TAG		"centronics"
#define FLOPPY_TIMER_TAG	"motor_off"

#define BW12_VIDEORAM_MASK	0x7ff
#define BW12_CHARROM_MASK	0xfff

class bw12_state : public driver_device
{
public:
	bw12_state(running_machine &machine, const driver_device_config_base &config)
		: driver_device(machine, config),
		  m_maincpu(*this, Z80_TAG),
		  m_pia(*this, PIA6821_TAG),
		  m_fdc(*this, UPD765_TAG),
		  m_kbc(*this, AY3600_TAG),
		  m_crtc(*this, MC6845_TAG),
		  m_centronics(*this, CENTRONICS_TAG),
		  m_ram(*this, "messram"),
		  m_floppy0(*this, FLOPPY_0),
		  m_floppy1(*this, FLOPPY_1),
		  m_floppy_timer(*this, FLOPPY_TIMER_TAG)
	{ }

	required_device<cpu_device> m_maincpu;
	required_device<running_device> m_pia;
	required_device<running_device> m_crtc;
	required_device<running_device> m_fdc;
	required_device<running_device> m_kbc;
	required_device<running_device> m_centronics;
	required_device<running_device> m_ram;
	required_device<running_device> m_floppy0;
	required_device<running_device> m_floppy1;
	required_device<timer_device> m_floppy_timer;
	
	virtual void machine_start();
	virtual void machine_reset();

	virtual void video_start();
	virtual bool video_update(screen_device &screen, bitmap_t &bitmap, const rectangle &cliprect);

	void bankswitch();
	void floppy_motor_off();
	void set_floppy_motor_off_timer();
	void ls259_w(int address, int data);

	DECLARE_READ8_MEMBER( ls259_r );
	DECLARE_WRITE8_MEMBER( ls259_w );
	DECLARE_WRITE_LINE_MEMBER( fdc_intrq_w );
	DECLARE_READ8_MEMBER( pia_pa_r );
	DECLARE_READ_LINE_MEMBER( pia_cb1_r );
	DECLARE_WRITE_LINE_MEMBER( pia_cb2_w );
	DECLARE_WRITE_LINE_MEMBER( pit_out2_w );
	DECLARE_WRITE_LINE_MEMBER( ay3600_data_ready_w );

	/* memory state */
	int m_bank;

	/* video state */
	UINT8 *m_video_ram;
	UINT8 *m_char_rom;

	/* PIT state */
	int m_pit_out2;

	/* keyboard state */
	int m_key_data[9];
	int m_key_sin;
	int m_key_stb;
	int m_key_shift;

	/* floppy state */
	int m_fdc_int;
	int m_motor_on;
	int m_motor0;
	int m_motor1;
};

#endif
