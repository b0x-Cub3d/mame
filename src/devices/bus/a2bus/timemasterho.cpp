// license:BSD-3-Clause
// copyright-holders:R. Belmont
/*********************************************************************

    timemasterho.c

    Implemention of the Applied Engineering TimeMaster H.O.


    PCB Layout:
     _____________________________________________________
    |  ___       _            _____________________       |
    | | D |     | |MSM5832   |                     |u3    |
    | | I |   u1| |          |    HD46821P         | ___  |
    | | P |     |_|          |_____________________|| B | |
    | |_S_|                           _____         | A | |
    |                              u2|_____| 74LS245| T | |
    |  |J1    74LS00  74LS08         ____________   | T | |
    |  |         _     _          u4|            |  | E | |
    |  |      u5| | u6| |           |   2716     |  | R | |
    |  |        |_|   |_|           |____________|  |_Y_| |
    |____________________________                        _|
                                 |                      |
                                 |______________________|


    DIPS: 1:SET  2:MODE 3:NMI 4:IRQ
    1 & 4 are on by default.

    J1: 8 pins for X10 home control functions (top to bottom)
       1: ADJ  2: 5V  3: MODE  4: GND
       5: A    6: 5V  7: B     8: GND

    X10 functions not supported.

*********************************************************************/

#include "emu.h"
#include "timemasterho.h"

#include "machine/6821pia.h"
#include "machine/msm5832.h"


namespace {

/***************************************************************************
    PARAMETERS
***************************************************************************/

#define TIMEMASTER_ROM_REGION   "timemst_rom"
#define TIMEMASTER_PIA_TAG      "timemst_pia"
#define TIMEMASTER_M5832_TAG    "timemst_msm"

ROM_START( timemaster )
	ROM_REGION(0x1000, TIMEMASTER_ROM_REGION, 0)
	ROM_LOAD( "ae timemaster ii h.o. rom rev. 5.bin", 0x000000, 0x001000, CRC(ff5bd644) SHA1(ae0173da61581a06188c1bee89e95a0aa536c411) )
ROM_END

static INPUT_PORTS_START( tmho )
	PORT_START("DSW1")
	PORT_DIPNAME( 0x01, 0x01, "Set")
	PORT_DIPSETTING(    0x00, "Apple can't set clock")
	PORT_DIPSETTING(    0x01, "Apple can set clock")

	PORT_DIPNAME( 0x02, 0x00, "Mode")
	PORT_DIPSETTING(    0x00, "TimeMaster")
	PORT_DIPSETTING(    0x02, "Mountain AppleClock")

	PORT_DIPNAME( 0x04, 0x00, "NMI")
	PORT_DIPSETTING(    0x00,  DEF_STR(Off))
	PORT_DIPSETTING(    0x04,  DEF_STR(On))

	PORT_DIPNAME( 0x08, 0x08, "IRQ")
	PORT_DIPSETTING(    0x00,  DEF_STR(Off))
	PORT_DIPSETTING(    0x08,  DEF_STR(On))
INPUT_PORTS_END

//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class a2bus_timemasterho_device:
	public device_t,
	public device_a2bus_card_interface
{
public:
	// construction/destruction
	a2bus_timemasterho_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	a2bus_timemasterho_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);

	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;
	virtual void device_add_mconfig(machine_config &config) override ATTR_COLD;
	virtual const tiny_rom_entry *device_rom_region() const override ATTR_COLD;
	virtual ioport_constructor device_input_ports() const override ATTR_COLD;

	// overrides of standard a2bus slot functions
	virtual uint8_t read_c0nx(uint8_t offset) override;
	virtual void write_c0nx(uint8_t offset, uint8_t data) override;
	virtual uint8_t read_cnxx(uint8_t offset) override;
	virtual uint8_t read_c800(uint16_t offset) override;

	required_device<pia6821_device> m_pia;
	required_device<msm5832_device> m_msm5832;
	required_region_ptr<uint8_t> m_rom;
	required_ioport m_dsw1;

private:
	void update_irqs();
	void pia_out_a(uint8_t data);
	void pia_out_b(uint8_t data);
	void pia_irqa_w(int state);
	void pia_irqb_w(int state);

	bool m_irqa = false, m_irqb = false;
	bool m_started = false;
};

/***************************************************************************
    FUNCTION PROTOTYPES
***************************************************************************/

//-------------------------------------------------
//  input_ports - device-specific input ports
//-------------------------------------------------

ioport_constructor a2bus_timemasterho_device::device_input_ports() const
{
	return INPUT_PORTS_NAME( tmho );
}

//-------------------------------------------------
//  device_add_mconfig - add device configuration
//-------------------------------------------------

void a2bus_timemasterho_device::device_add_mconfig(machine_config &config)
{
	PIA6821(config, m_pia, 1021800);
	m_pia->writepa_handler().set(FUNC(a2bus_timemasterho_device::pia_out_a));
	m_pia->writepb_handler().set(FUNC(a2bus_timemasterho_device::pia_out_b));
	m_pia->irqa_handler().set(FUNC(a2bus_timemasterho_device::pia_irqa_w));
	m_pia->irqb_handler().set(FUNC(a2bus_timemasterho_device::pia_irqb_w));

	MSM5832(config, m_msm5832, 32768);
}

//-------------------------------------------------
//  rom_region - device-specific ROM region
//-------------------------------------------------

const tiny_rom_entry *a2bus_timemasterho_device::device_rom_region() const
{
	return ROM_NAME( timemaster );
}

//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

a2bus_timemasterho_device::a2bus_timemasterho_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, type, tag, owner, clock),
	device_a2bus_card_interface(mconfig, *this),
	m_pia(*this, TIMEMASTER_PIA_TAG),
	m_msm5832(*this, TIMEMASTER_M5832_TAG),
	m_rom(*this, TIMEMASTER_ROM_REGION),
	m_dsw1(*this, "DSW1")
{
}

a2bus_timemasterho_device::a2bus_timemasterho_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	a2bus_timemasterho_device(mconfig, A2BUS_TIMEMASTERHO, tag, owner, clock)
{
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void a2bus_timemasterho_device::device_start()
{
}

void a2bus_timemasterho_device::device_reset()
{
	m_msm5832->cs_w(ASSERT_LINE);   // CS is tied to Vcc
	m_started = true;
}


/*-------------------------------------------------
    read_c0nx - called for reads from this card's c0nx space
-------------------------------------------------*/

uint8_t a2bus_timemasterho_device::read_c0nx(uint8_t offset)
{
	if (offset <= 3)
	{
		return m_pia->read(offset);
	}

	return 0xff;
}


/*-------------------------------------------------
    write_c0nx - called for writes to this card's c0nx space
-------------------------------------------------*/

void a2bus_timemasterho_device::write_c0nx(uint8_t offset, uint8_t data)
{
	if (offset <= 3)
	{
		m_pia->write(offset, data);
	}
}

/*-------------------------------------------------
    read_cnxx - called for reads from this card's cnxx space
-------------------------------------------------*/

uint8_t a2bus_timemasterho_device::read_cnxx(uint8_t offset)
{
	if (m_started)
	{
		if (!(m_dsw1->read() & 2))  // TimeMaster native
		{
			return m_rom[offset+0xc00];
		}
	}

	// Mountain Computer compatible
	return m_rom[offset+0x800];
}

/*-------------------------------------------------
    read_c800 - called for reads from this card's c800 space
-------------------------------------------------*/

uint8_t a2bus_timemasterho_device::read_c800(uint16_t offset)
{
	return m_rom[offset+0xc00];
}

void a2bus_timemasterho_device::pia_out_a(uint8_t data)
{
	// port A appears to be input only
}

void a2bus_timemasterho_device::pia_out_b(uint8_t data)
{
	m_msm5832->address_w(data & 0xf);
	m_msm5832->hold_w((data>>4) & 1 ? ASSERT_LINE : CLEAR_LINE);
	m_msm5832->read_w((data>>5) & 1 ? ASSERT_LINE : CLEAR_LINE);

	if (m_started)
	{
		if (m_dsw1->read() & 1)
		{
			m_msm5832->write_w((data >> 6) & 1 ? ASSERT_LINE : CLEAR_LINE);
		}
	}

	// if it's a read, poke it into the PIA
	if ((data>>5) & 1)
	{
		m_pia->porta_w(m_msm5832->data_r());
	}
}

void a2bus_timemasterho_device::update_irqs()
{
	uint8_t dip = 0;

	if (m_started)
	{
		dip = m_dsw1->read();
	}

	if ((m_irqa | m_irqb) == ASSERT_LINE)
	{
		if (dip & 4)
		{
			raise_slot_nmi();
		}
		if (dip & 8)
		{
			raise_slot_irq();
		}
	}
	else
	{
		lower_slot_irq();
		lower_slot_nmi();
	}
}

void a2bus_timemasterho_device::pia_irqa_w(int state)
{
	m_irqa = state;
	update_irqs();
}

void a2bus_timemasterho_device::pia_irqb_w(int state)
{
	m_irqb = state;
	update_irqs();
}

} // anonymous namespace


//**************************************************************************
//  GLOBAL VARIABLES
//**************************************************************************

DEFINE_DEVICE_TYPE_PRIVATE(A2BUS_TIMEMASTERHO, device_a2bus_card_interface, a2bus_timemasterho_device, "a2tmstho", "Applied Engineering TimeMaster H.O.")
