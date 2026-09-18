/*
 * Copyright 2022-2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Fixed PV198 board configuration. */
/*
 * How to setup clock using clock driver functions:
 *
 * 1. Setup clock sources.
 *
 * 2. Set up wait states of the flash.
 *
 * 3. Set up all dividers.
 *
 * 4. Set up all selectors to provide selected clocks.
 *
 */


#include "fsl_clock.h"
#include "clock_config_internal.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
/* System clock frequency. */
extern uint32_t SystemCoreClock;

/*******************************************************************************
 ************************ BOARD_InitBootClocks function ************************
 ******************************************************************************/
void BOARD_InitBootClocks(void)
{
    BOARD_BootClockPLL150M();
}

/*******************************************************************************
 ******************** Configuration BOARD_BootClockFRO12M **********************
 ******************************************************************************/

/*******************************************************************************
 * Variables for BOARD_BootClockFRO12M configuration
 ******************************************************************************/
/*******************************************************************************
 * Code for BOARD_BootClockFRO12M configuration
 ******************************************************************************/
void BOARD_BootClockFRO12M(void)
{
    CLOCK_EnableClock(kCLOCK_Scg);                     /*!< Enable SCG clock */

    CLOCK_SetFLASHAccessCyclesForFreq(12000000U, kOD_Mode);           /*!< Set the additional number of flash wait-states */

    /*!< Set up clock selectors - Attach clocks to the peripheries */
    CLOCK_AttachClk(kFRO12M_to_MAIN_CLK);                 /*!< Switch MAIN_CLK to FRO12M */

    /*!< Set up dividers */
    CLOCK_SetClkDiv(kCLOCK_DivAhbClk, 1U);           /*!< Set AHBCLKDIV divider to value 1 */

    /* Set SystemCoreClock variable */
    SystemCoreClock = BOARD_BOOTCLOCKFRO12M_CORE_CLOCK;
}

/*******************************************************************************
 ******************* Configuration BOARD_BootClockFROHF48M *********************
 ******************************************************************************/

/*******************************************************************************
 * Variables for BOARD_BootClockFROHF48M configuration
 ******************************************************************************/
/*******************************************************************************
 * Code for BOARD_BootClockFROHF48M configuration
 ******************************************************************************/
void BOARD_BootClockFROHF48M(void)
{
    CLOCK_EnableClock(kCLOCK_Scg);                     /*!< Enable SCG clock */

    /* FRO OSC setup - begin, enable the FRO for safety switching */
    CLOCK_AttachClk(kFRO12M_to_MAIN_CLK);              /*!< Switch to FRO 12M first to ensure we can change the clock setting */

    CLOCK_SetFLASHAccessCyclesForFreq(48000000U, kOD_Mode);           /*!< Set the additional number of flash wait-states */

    CLOCK_SetupFROHFClocking(48000000U);               /*!< Enable FRO HF(48MHz) output */
    /*!< Set up clock selectors - Attach clocks to the peripheries */
    CLOCK_AttachClk(kFRO_HF_to_MAIN_CLK);                 /*!< Switch MAIN_CLK to FRO_HF */

    /*!< Set up dividers */
    CLOCK_SetClkDiv(kCLOCK_DivAhbClk, 1U);           /*!< Set AHBCLKDIV divider to value 1 */

    /* Set SystemCoreClock variable */
    SystemCoreClock = BOARD_BOOTCLOCKFROHF48M_CORE_CLOCK;
}

/*******************************************************************************
 ******************* Configuration BOARD_BootClockFROHF144M ********************
 ******************************************************************************/

/*******************************************************************************
 * Variables for BOARD_BootClockFROHF144M configuration
 ******************************************************************************/
/*******************************************************************************
 * Code for BOARD_BootClockFROHF144M configuration
 ******************************************************************************/
void BOARD_BootClockFROHF144M(void)
{
    CLOCK_EnableClock(kCLOCK_Scg);                     /*!< Enable SCG clock */

    /* FRO OSC setup - begin, enable the FRO for safety switching */
    CLOCK_AttachClk(kFRO12M_to_MAIN_CLK);              /*!< Switch to FRO 12M first to ensure we can change the clock setting */

    CLOCK_SetFLASHAccessCyclesForFreq(144000000U, kOD_Mode);           /*!< Set the additional number of flash wait-states */

    CLOCK_SetupFROHFClocking(144000000U);               /*!< Enable FRO HF(144MHz) output */
    /*!< Set up clock selectors - Attach clocks to the peripheries */
    CLOCK_AttachClk(kFRO_HF_to_MAIN_CLK);                 /*!< Switch MAIN_CLK to FRO_HF */

    /*!< Set up dividers */
    CLOCK_SetClkDiv(kCLOCK_DivAhbClk, 1U);           /*!< Set AHBCLKDIV divider to value 1 */

    /* Set SystemCoreClock variable */
    SystemCoreClock = BOARD_BOOTCLOCKFROHF144M_CORE_CLOCK;
}

/*******************************************************************************
 ******************** Configuration BOARD_BootClockPLL150M *********************
 ******************************************************************************/

/*******************************************************************************
 * Variables for BOARD_BootClockPLL150M configuration
 ******************************************************************************/
/*******************************************************************************
 * Code for BOARD_BootClockPLL150M configuration
 ******************************************************************************/
void BOARD_BootClockPLL150M(void)
{
    CLOCK_EnableClock(kCLOCK_Scg);                     /*!< Enable SCG clock */

    /* FRO OSC setup - begin, enable the FRO for safety switching */
    CLOCK_AttachClk(kFRO12M_to_MAIN_CLK);              /*!< Switch to FRO 12M first to ensure we can change the clock setting */

    CLOCK_SetFLASHAccessCyclesForFreq(150000000U, kOD_Mode);           /*!< Set the additional number of flash wait-states */

    CLOCK_SetupFROHFClocking(48000000U);               /*!< Enable FRO HF(48MHz) output */
    /*!< Set up PLL0 */
    const pll_setup_t pll0Setup = {
        .pllctrl = SCG_APLLCTRL_SOURCE(1U) | SCG_APLLCTRL_SELI(27U) | SCG_APLLCTRL_SELP(13U),
        .pllndiv = SCG_APLLNDIV_NDIV(8U),
        .pllpdiv = SCG_APLLPDIV_PDIV(1U),
        .pllmdiv = SCG_APLLMDIV_MDIV(50U),
        .pllRate = 150000000U
    };
    CLOCK_SetPLL0Freq(&pll0Setup);                       /*!< Configure PLL0 to the desired values */
    CLOCK_SetPll0MonitorMode(kSCG_Pll0MonitorDisable);    /* Pll0 Monitor is disabled */

    /*!< Set up clock selectors - Attach clocks to the peripheries */
    CLOCK_AttachClk(kPLL0_to_MAIN_CLK);                 /*!< Switch MAIN_CLK to PLL0 */

    /*!< Set up dividers */
    CLOCK_SetClkDiv(kCLOCK_DivAhbClk, 1U);           /*!< Set AHBCLKDIV divider to value 1 */

    /* Set SystemCoreClock variable */
    SystemCoreClock = BOARD_BOOTCLOCKPLL150M_CORE_CLOCK;
}

/*******************************************************************************
 ******************** Configuration BOARD_BootClockPLL100M *********************
 ******************************************************************************/

/*******************************************************************************
 * Variables for BOARD_BootClockPLL100M configuration
 ******************************************************************************/
/*******************************************************************************
 * Code for BOARD_BootClockPLL100M configuration
 ******************************************************************************/
void BOARD_BootClockPLL100M(void)
{
    CLOCK_EnableClock(kCLOCK_Scg);                     /*!< Enable SCG clock */

    /* FRO OSC setup - begin, enable the FRO for safety switching */
    CLOCK_AttachClk(kFRO12M_to_MAIN_CLK);              /*!< Switch to FRO 12M first to ensure we can change the clock setting */

    CLOCK_SetFLASHAccessCyclesForFreq(100000000U, kOD_Mode);           /*!< Set the additional number of flash wait-states */

    CLOCK_SetupExtClocking(24000000U);
    CLOCK_SetSysOscMonitorMode(kSCG_SysOscMonitorDisable);    /* System OSC Clock Monitor is disabled */

    /*!< Set up PLL1 */
    const pll_setup_t pll1Setup = {
        .pllctrl = SCG_SPLLCTRL_SOURCE(0U) | SCG_SPLLCTRL_SELI(53U) | SCG_SPLLCTRL_SELP(26U),
        .pllndiv = SCG_SPLLNDIV_NDIV(6U),
        .pllpdiv = SCG_SPLLPDIV_PDIV(2U),
        .pllmdiv = SCG_SPLLMDIV_MDIV(100U),
        .pllRate = 100000000U
    };
    CLOCK_SetPLL1Freq(&pll1Setup);                       /*!< Configure PLL1 to the desired values */
    CLOCK_SetPll1MonitorMode(kSCG_Pll1MonitorDisable);    /* Pll1 Monitor is disabled */

    /*!< Set up clock selectors - Attach clocks to the peripheries */
    CLOCK_AttachClk(kPLL1_to_MAIN_CLK);                 /*!< Switch MAIN_CLK to PLL1 */

    /*!< Set up dividers */
    CLOCK_SetClkDiv(kCLOCK_DivAhbClk, 1U);           /*!< Set AHBCLKDIV divider to value 1 */

    /* Set SystemCoreClock variable */
    SystemCoreClock = BOARD_BOOTCLOCKPLL100M_CORE_CLOCK;
}

