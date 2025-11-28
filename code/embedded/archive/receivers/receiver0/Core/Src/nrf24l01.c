/*
------------------------------------------------------------------------------
~ File   : nrf24l01.c (multi-instance refactor)
~ Notes  : Each NRF24L01_HandleTypeDef can use a different SPI bus and CE/CS pins
------------------------------------------------------------------------------
*/

#include "nrf24l01.h"

/* ------------------------------- Local helpers ------------------------------- */

static uint8_t _read_reg(NRF24L01_HandleTypeDef *h, uint8_t reg, uint16_t Timeout)
{
    uint8_t cmd    = NRF24L01_CMD_R_REGISTER | reg;
    uint8_t status = 0;
    uint8_t value  = 0;

#if defined(USE_HAL_DRIVER)
    HAL_GPIO_WritePin(h->CS_GPIO_PORT, h->CS_GPIO_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(h->hspi, &cmd, &status, 1, Timeout);
    HAL_SPI_Receive(h->hspi, &value, 1, Timeout);
    HAL_GPIO_WritePin(h->CS_GPIO_PORT, h->CS_GPIO_PIN, GPIO_PIN_SET);
#else
    /* Replace with your platform SPI/GPIO */
#endif
    (void)status;
    return value;
}

static void _write_reg(NRF24L01_HandleTypeDef *h, uint8_t reg, uint8_t data, uint16_t Timeout)
{
    uint8_t cmd    = NRF24L01_CMD_W_REGISTER | reg;
    uint8_t status = 0;

#if defined(USE_HAL_DRIVER)
    HAL_GPIO_WritePin(h->CS_GPIO_PORT, h->CS_GPIO_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(h->hspi, &cmd, &status, 1, Timeout);
    HAL_SPI_Transmit(h->hspi, &data, 1, Timeout);
    HAL_GPIO_WritePin(h->CS_GPIO_PORT, h->CS_GPIO_PIN, GPIO_PIN_SET);
#else
    /* Replace with your platform SPI/GPIO */
#endif
    (void)status;
}

static void _write_reg_multi(NRF24L01_HandleTypeDef *h, uint8_t reg, uint8_t *data, uint16_t size, uint16_t Timeout)
{
    uint8_t cmd    = NRF24L01_CMD_W_REGISTER | reg;
    uint8_t status = 0;

#if defined(USE_HAL_DRIVER)
    HAL_GPIO_WritePin(h->CS_GPIO_PORT, h->CS_GPIO_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(h->hspi, &cmd, &status, 1, Timeout);
    HAL_SPI_Transmit(h->hspi, data, size, Timeout);
    HAL_GPIO_WritePin(h->CS_GPIO_PORT, h->CS_GPIO_PIN, GPIO_PIN_SET);
#else
    /* Replace with your platform SPI/GPIO */
#endif
    (void)status;
}

/* ---------------------------------- Init ------------------------------------ */

void NRF24L01_RxInit(NRF24L01_HandleTypeDef *h, uint8_t Channel, uint8_t DataRate, uint16_t Timeout)
{
    NRF24L01_Reset(h, Timeout);

    NRF24L01_SetRxPayloadWidths(h, NRF24L01_PAYLOAD_LENGTH, Timeout);
    NRF24L01_SetRFChannel(h, Channel, Timeout);
    NRF24L01_SetRFAirDataRate(h, (NRF24L01_DataRateTypeDef)DataRate, Timeout);
    NRF24L01_SetRFTxOutputPower(h, NRF24L01_PWR_0DBM, Timeout);

    NRF24L01_SetCRCLength(h, 1, Timeout);
    NRF24L01_SetAutoACK(h, NRF24L01_AUTO_ACK_ALL, NRF24L01_ENABLE, Timeout);
    NRF24L01_EnableRxAddress(h, NRF24L01_RX_ADDRESS_ALL, Timeout);
    NRF24L01_SetAddressWidths(h, NRF24L01_ADDRESS_WIDTH, Timeout);
    NRF24L01_AutoRetransmitCount(h, 3, Timeout);
    NRF24L01_AutoRetransmitDelay(h, 250, Timeout);

    NRF24L01_PRxMode(h, Timeout);
    NRF24L01_PowerUp(h, Timeout);

#if defined(USE_HAL_DRIVER)
    HAL_GPIO_WritePin(h->CE_GPIO_PORT, h->CE_GPIO_PIN, GPIO_PIN_SET);
#endif
}

void NRF24L01_TxInit(NRF24L01_HandleTypeDef *h, uint8_t Channel, uint8_t DataRate, uint16_t Timeout)
{
    NRF24L01_Reset(h, Timeout);

    NRF24L01_SetRFChannel(h, Channel, Timeout);
    NRF24L01_SetRFAirDataRate(h, (NRF24L01_DataRateTypeDef)DataRate, Timeout);
    NRF24L01_SetRFTxOutputPower(h, NRF24L01_PWR_0DBM, Timeout);

    NRF24L01_SetCRCLength(h, 1, Timeout);
    NRF24L01_SetAutoACK(h, NRF24L01_AUTO_ACK_ALL, NRF24L01_ENABLE, Timeout);
    NRF24L01_EnableRxAddress(h, NRF24L01_RX_ADDRESS_ALL, Timeout);
    NRF24L01_SetAddressWidths(h, NRF24L01_ADDRESS_WIDTH, Timeout);
    NRF24L01_AutoRetransmitCount(h, 3, Timeout);
    NRF24L01_AutoRetransmitDelay(h, 250, Timeout);

    NRF24L01_PTxMode(h, Timeout);
    NRF24L01_PowerUp(h, Timeout);

#if defined(USE_HAL_DRIVER)
    HAL_GPIO_WritePin(h->CE_GPIO_PORT, h->CE_GPIO_PIN, GPIO_PIN_SET);
#endif
}

/* --------------------------------- Control ---------------------------------- */

void NRF24L01_Reset(NRF24L01_HandleTypeDef *h, uint16_t Timeout)
{
#if defined(USE_HAL_DRIVER)
    HAL_GPIO_WritePin(h->CS_GPIO_PORT, h->CS_GPIO_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(h->CE_GPIO_PORT, h->CE_GPIO_PIN, GPIO_PIN_RESET);
#endif

    /* Default registers */
    _write_reg(h, NRF24L01_REG_CONFIG,      NRF24L01_REG_DEF_CONFIG,      Timeout);
    _write_reg(h, NRF24L01_REG_EN_AA,       NRF24L01_REG_DEF_EN_AA,       Timeout);
    _write_reg(h, NRF24L01_REG_EN_RXADDR,   NRF24L01_REG_DEF_EN_RXADDR,   Timeout);
    _write_reg(h, NRF24L01_REG_SETUP_AW,    NRF24L01_REG_DEF_SETUP_AW,    Timeout);
    _write_reg(h, NRF24L01_REG_SETUP_RETR,  NRF24L01_REG_DEF_SETUP_RETR,  Timeout);
    _write_reg(h, NRF24L01_REG_RF_CH,       NRF24L01_REG_DEF_RF_CH,       Timeout);
    _write_reg(h, NRF24L01_REG_RF_SETUP,    NRF24L01_REG_DEF_RF_SETUP,    Timeout);
    _write_reg(h, NRF24L01_REG_STATUS,      NRF24L01_REG_DEF_STATUS,      Timeout);

    /* RX pipe 0 / TX address */
    uint8_t p0[NRF24L01_ADDRESS_WIDTH] = {
        NRF24L01_REG_DEF_RX_ADDR_P0,
        NRF24L01_REG_DEF_RX_ADDR_P0,
        NRF24L01_REG_DEF_RX_ADDR_P0,
        NRF24L01_REG_DEF_RX_ADDR_P0,
        NRF24L01_REG_DEF_RX_ADDR_P0
    };
    _write_reg_multi(h, NRF24L01_REG_RX_ADDR_P0, p0, NRF24L01_ADDRESS_WIDTH, Timeout);
    _write_reg_multi(h, NRF24L01_REG_TX_ADDR,    p0, NRF24L01_ADDRESS_WIDTH, Timeout);

    /* RX pipe 1 */
    uint8_t p1[NRF24L01_ADDRESS_WIDTH] = {
        NRF24L01_REG_DEF_RX_ADDR_P1,
        NRF24L01_REG_DEF_RX_ADDR_P1,
        NRF24L01_REG_DEF_RX_ADDR_P1,
        NRF24L01_REG_DEF_RX_ADDR_P1,
        NRF24L01_REG_DEF_RX_ADDR_P1
    };
    _write_reg_multi(h, NRF24L01_REG_RX_ADDR_P1, p1, NRF24L01_ADDRESS_WIDTH, Timeout);

    /* RX pipe 2-5 */
    _write_reg(h, NRF24L01_REG_RX_ADDR_P2, NRF24L01_REG_DEF_RX_ADDR_P2, Timeout);
    _write_reg(h, NRF24L01_REG_RX_ADDR_P3, NRF24L01_REG_DEF_RX_ADDR_P3, Timeout);
    _write_reg(h, NRF24L01_REG_RX_ADDR_P4, NRF24L01_REG_DEF_RX_ADDR_P4, Timeout);
    _write_reg(h, NRF24L01_REG_RX_ADDR_P5, NRF24L01_REG_DEF_RX_ADDR_P5, Timeout);

    _write_reg(h, NRF24L01_REG_RX_PW_P0,    NRF24L01_REG_DEF_RX_PW_P0_P5, Timeout);
    _write_reg(h, NRF24L01_REG_RX_PW_P1,    NRF24L01_REG_DEF_RX_PW_P0_P5, Timeout);
    _write_reg(h, NRF24L01_REG_RX_PW_P2,    NRF24L01_REG_DEF_RX_PW_P0_P5, Timeout);
    _write_reg(h, NRF24L01_REG_RX_PW_P3,    NRF24L01_REG_DEF_RX_PW_P0_P5, Timeout);
    _write_reg(h, NRF24L01_REG_RX_PW_P4,    NRF24L01_REG_DEF_RX_PW_P0_P5, Timeout);
    _write_reg(h, NRF24L01_REG_RX_PW_P5,    NRF24L01_REG_DEF_RX_PW_P0_P5, Timeout);
    _write_reg(h, NRF24L01_REG_FIFO_STATUS, NRF24L01_REG_DEF_FIFO_STATUS, Timeout);
    _write_reg(h, NRF24L01_REG_DYNPD,       NRF24L01_REG_DEF_DYNPD,       Timeout);
    _write_reg(h, NRF24L01_REG_FEATURE,     NRF24L01_REG_DEF_FEATURE,     Timeout);

    NRF24L01_FlushRxFIFO(h, Timeout);
    NRF24L01_FlushTxFIFO(h, Timeout);
}

/* ------------------------------- Primary mode -------------------------------- */

void NRF24L01_PRxMode(NRF24L01_HandleTypeDef *h, uint16_t Timeout)
{
    uint8_t regVal = _read_reg(h, NRF24L01_REG_CONFIG, Timeout);
    __NRF24L01_SET_BIT(regVal, NRF24L01_PRIM_RX);
    _write_reg(h, NRF24L01_REG_CONFIG, regVal, Timeout);
}

void NRF24L01_PTxMode(NRF24L01_HandleTypeDef *h, uint16_t Timeout)
{
    uint8_t regVal = _read_reg(h, NRF24L01_REG_CONFIG, Timeout);
    __NRF24L01_RESET_BIT(regVal, NRF24L01_PRIM_RX);
    _write_reg(h, NRF24L01_REG_CONFIG, regVal, Timeout);
}

/* --------------------------- Transmit / Receive ------------------------------ */

void NRF24L01_RxReceive(NRF24L01_HandleTypeDef *h, uint8_t *pRxPayload, uint8_t *RxPayloadNumber, uint16_t Timeout)
{
    *RxPayloadNumber = (_read_reg(h, NRF24L01_REG_STATUS, Timeout) >> 1) & 0x07;
    NRF24L01_ReadRxFIFO(h, pRxPayload, Timeout);
    NRF24L01_ClearRxDR(h, Timeout);
}

void NRF24L01_TxTransmit(NRF24L01_HandleTypeDef *h, uint8_t *pTxPayload, uint16_t Timeout)
{
    NRF24L01_WriteTxFIFO(h, pTxPayload, Timeout);
}

/* Rx/Tx Status control */

void NRF24L01_ClearRxDR(NRF24L01_HandleTypeDef *h, uint16_t Timeout)
{
    uint8_t status = NRF24L01_GetStatus(h, Timeout);
    __NRF24L01_SET_BIT(status, NRF24L01_RX_DR);
    _write_reg(h, NRF24L01_REG_STATUS, status, Timeout);
}

void NRF24L01_ClearTxDS(NRF24L01_HandleTypeDef *h, uint16_t Timeout)
{
    uint8_t status = NRF24L01_GetStatus(h, Timeout);
    __NRF24L01_SET_BIT(status, NRF24L01_TX_DS);
    _write_reg(h, NRF24L01_REG_STATUS, status, Timeout);
}

void NRF24L01_ClearMaxRT(NRF24L01_HandleTypeDef *h, uint16_t Timeout)
{
    uint8_t status = NRF24L01_GetStatus(h, Timeout);
    __NRF24L01_SET_BIT(status, NRF24L01_MAX_RT);
    _write_reg(h, NRF24L01_REG_STATUS, status, Timeout);
}

/* -------------------------------- FIFO control -------------------------------- */

uint8_t NRF24L01_ReadRxFIFO(NRF24L01_HandleTypeDef *h, uint8_t *pRxPayload, uint16_t Timeout)
{
    uint8_t cmd    = NRF24L01_CMD_R_RX_PAYLOAD;
    uint8_t status = 0;

#if defined(USE_HAL_DRIVER)
    HAL_GPIO_WritePin(h->CS_GPIO_PORT, h->CS_GPIO_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(h->hspi, &cmd, &status, 1, Timeout);
    HAL_SPI_Receive(h->hspi, pRxPayload, NRF24L01_PAYLOAD_LENGTH, Timeout);
    HAL_GPIO_WritePin(h->CS_GPIO_PORT, h->CS_GPIO_PIN, GPIO_PIN_SET);
#endif
    return status;
}

uint8_t NRF24L01_WriteTxFIFO(NRF24L01_HandleTypeDef *h, uint8_t *pTxPayload, uint16_t Timeout)
{
    uint8_t cmd    = NRF24L01_CMD_W_TX_PAYLOAD;
    uint8_t status = 0;

#if defined(USE_HAL_DRIVER)
    HAL_GPIO_WritePin(h->CS_GPIO_PORT, h->CS_GPIO_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(h->hspi, &cmd, &status, 1, Timeout);
    HAL_SPI_Transmit(h->hspi, pTxPayload, NRF24L01_PAYLOAD_LENGTH, Timeout);
    HAL_GPIO_WritePin(h->CS_GPIO_PORT, h->CS_GPIO_PIN, GPIO_PIN_SET);
#endif
    return status;
}

void NRF24L01_FlushRxFIFO(NRF24L01_HandleTypeDef *h, uint16_t Timeout)
{
    uint8_t cmd    = NRF24L01_CMD_FLUSH_RX;
    uint8_t status = 0;
#if defined(USE_HAL_DRIVER)
    HAL_GPIO_WritePin(h->CS_GPIO_PORT, h->CS_GPIO_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(h->hspi, &cmd, &status, 1, Timeout);
    HAL_GPIO_WritePin(h->CS_GPIO_PORT, h->CS_GPIO_PIN, GPIO_PIN_SET);
#endif
    (void)status;
}

void NRF24L01_FlushTxFIFO(NRF24L01_HandleTypeDef *h, uint16_t Timeout)
{
    uint8_t cmd    = NRF24L01_CMD_FLUSH_TX;
    uint8_t status = 0;
#if defined(USE_HAL_DRIVER)
    HAL_GPIO_WritePin(h->CS_GPIO_PORT, h->CS_GPIO_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(h->hspi, &cmd, &status, 1, Timeout);
    HAL_GPIO_WritePin(h->CS_GPIO_PORT, h->CS_GPIO_PIN, GPIO_PIN_SET);
#endif
    (void)status;
}

uint8_t NRF24L01_GetFIFOStatus(NRF24L01_HandleTypeDef *h, uint16_t Timeout)
{
    return _read_reg(h, NRF24L01_REG_FIFO_STATUS, Timeout);
}

/* ----------------------------------- Power ----------------------------------- */

void NRF24L01_PowerUp(NRF24L01_HandleTypeDef *h, uint16_t Timeout)
{
    uint8_t regVal = _read_reg(h, NRF24L01_REG_CONFIG, Timeout);
    __NRF24L01_SET_BIT(regVal, NRF24L01_PWR_UP);
    _write_reg(h, NRF24L01_REG_CONFIG, regVal, Timeout);
}

void NRF24L01_PowerDown(NRF24L01_HandleTypeDef *h, uint16_t Timeout)
{
    uint8_t regVal = _read_reg(h, NRF24L01_REG_CONFIG, Timeout);
    __NRF24L01_RESET_BIT(regVal, NRF24L01_PWR_UP);
    _write_reg(h, NRF24L01_REG_CONFIG, regVal, Timeout);
}

/* --------------------------------- RF Address -------------------------------- */

void NRF24L01_EnableRxAddress(NRF24L01_HandleTypeDef *h, NRF24L01_RxAddTypeDef RxAddress, uint16_t Timeout)
{
    uint8_t regVal = _read_reg(h, NRF24L01_REG_EN_RXADDR, Timeout) | RxAddress;
    _write_reg(h, NRF24L01_REG_EN_RXADDR, regVal, Timeout);
}

void NRF24L01_DisableRxAddress(NRF24L01_HandleTypeDef *h, NRF24L01_RxAddTypeDef RxAddress, uint16_t Timeout)
{
    uint8_t regVal = _read_reg(h, NRF24L01_REG_EN_RXADDR, Timeout) & (~RxAddress);
    _write_reg(h, NRF24L01_REG_EN_RXADDR, regVal, Timeout);
}

void NRF24L01_SetAddressWidths(NRF24L01_HandleTypeDef *h, uint8_t Width, uint16_t Timeout)
{
    _write_reg(h, NRF24L01_REG_SETUP_AW, (Width - 2), Timeout);
}

void NRF24L01_SetRxAddress(NRF24L01_HandleTypeDef *h, NRF24L01_RxAddTypeDef RxAddress, uint8_t *Address, uint16_t Timeout)
{
    uint8_t _addressIndx = 0;
    uint8_t _address[NRF24L01_ADDRESS_WIDTH];

#if defined(USE_HAL_DRIVER)
    HAL_GPIO_WritePin(h->CE_GPIO_PORT, h->CE_GPIO_PIN, GPIO_PIN_RESET);
#endif

    switch (RxAddress)
    {
    case NRF24L01_RX_ADDRESS_P0:
    {
        for (; _addressIndx < NRF24L01_ADDRESS_WIDTH; _addressIndx++)
            _address[_addressIndx] = Address[NRF24L01_ADDRESS_WIDTH - _addressIndx - 1];

        _write_reg_multi(h, NRF24L01_REG_RX_ADDR_P0, _address, NRF24L01_ADDRESS_WIDTH, Timeout);
    } break;

    case NRF24L01_RX_ADDRESS_P1:
    {
        for (; _addressIndx < NRF24L01_ADDRESS_WIDTH; _addressIndx++)
            _address[_addressIndx] = Address[NRF24L01_ADDRESS_WIDTH - _addressIndx - 1];

        _write_reg_multi(h, NRF24L01_REG_RX_ADDR_P1, _address, NRF24L01_ADDRESS_WIDTH, Timeout);
    } break;

    case NRF24L01_RX_ADDRESS_P2:
        _write_reg(h, NRF24L01_REG_RX_ADDR_P2, Address[0], Timeout);
        break;
    case NRF24L01_RX_ADDRESS_P3:
        _write_reg(h, NRF24L01_REG_RX_ADDR_P3, Address[0], Timeout);
        break;
    case NRF24L01_RX_ADDRESS_P4:
        _write_reg(h, NRF24L01_REG_RX_ADDR_P4, Address[0], Timeout);
        break;
    case NRF24L01_RX_ADDRESS_P5:
        _write_reg(h, NRF24L01_REG_RX_ADDR_P5, Address[0], Timeout);
        break;
    default:
        break;
    }

#if defined(USE_HAL_DRIVER)
    HAL_GPIO_WritePin(h->CE_GPIO_PORT, h->CE_GPIO_PIN, GPIO_PIN_SET);
#endif
}

void NRF24L01_SetTxAddress(NRF24L01_HandleTypeDef *h, uint8_t *Address, uint16_t Timeout)
{
    uint8_t _addressIndx = 0;
    uint8_t _address[NRF24L01_ADDRESS_WIDTH];

    for (; _addressIndx < NRF24L01_ADDRESS_WIDTH; _addressIndx++)
        _address[_addressIndx] = Address[NRF24L01_ADDRESS_WIDTH - _addressIndx - 1];

    _write_reg_multi(h, NRF24L01_REG_RX_ADDR_P0, _address, NRF24L01_ADDRESS_WIDTH, Timeout);
    _write_reg_multi(h, NRF24L01_REG_TX_ADDR,   _address, NRF24L01_ADDRESS_WIDTH, Timeout);
}

/* ---------------------------------- RF Control -------------------------------- */

void NRF24L01_SetRxPayloadWidths(NRF24L01_HandleTypeDef *h, uint8_t Width, uint16_t Timeout)
{
    _write_reg(h, NRF24L01_REG_RX_PW_P0, Width, Timeout);
    _write_reg(h, NRF24L01_REG_RX_PW_P1, Width, Timeout);
    _write_reg(h, NRF24L01_REG_RX_PW_P2, Width, Timeout);
    _write_reg(h, NRF24L01_REG_RX_PW_P3, Width, Timeout);
    _write_reg(h, NRF24L01_REG_RX_PW_P4, Width, Timeout);
    _write_reg(h, NRF24L01_REG_RX_PW_P5, Width, Timeout);
}

void NRF24L01_SetCRCLength(NRF24L01_HandleTypeDef *h, uint8_t Length, uint16_t Timeout)
{
    uint8_t regVal = _read_reg(h, NRF24L01_REG_CONFIG, Timeout) & NRF24L01_CRCO_MASK_RST;

    __NRF24L01_SET_BIT(regVal, NRF24L01_EN_CRC);

    switch (Length)
    {
    case 1: __NRF24L01_RESET_BIT(regVal, NRF24L01_CRCO); break; /* 1 byte */
    case 2: __NRF24L01_SET_BIT(regVal, NRF24L01_CRCO);   break; /* 2 bytes */
    default: break;
    }

    _write_reg(h, NRF24L01_REG_CONFIG, regVal, Timeout);
}

void NRF24L01_SetAutoACK(NRF24L01_HandleTypeDef *h, NRF24L01_AutoACKTypeDef AACK, NRF24L01_StateTypeDef State, uint16_t Timeout)
{
    uint8_t regVal = _read_reg(h, NRF24L01_REG_EN_AA, Timeout);
    if (State == NRF24L01_ENABLE) regVal |= AACK; else regVal &= ~AACK;
    _write_reg(h, NRF24L01_REG_EN_AA, regVal, Timeout);
}

void NRF24L01_AutoRetransmitCount(NRF24L01_HandleTypeDef *h, uint8_t Count, uint16_t Timeout)
{
    uint8_t regVal = _read_reg(h, NRF24L01_REG_SETUP_RETR, Timeout) & NRF24L01_ARC_MASK_RST;
    regVal |= Count;
    _write_reg(h, NRF24L01_REG_SETUP_RETR, regVal, Timeout);
}

void NRF24L01_AutoRetransmitDelay(NRF24L01_HandleTypeDef *h, uint16_t DelayTimeUS, uint16_t Timeout)
{
    uint8_t regVal = _read_reg(h, NRF24L01_REG_SETUP_RETR, Timeout) & NRF24L01_ARD_BIT_MASK_RST;
    regVal |= (uint8_t)(((DelayTimeUS / 250U) - 1U) << 4);
    _write_reg(h, NRF24L01_REG_SETUP_RETR, regVal, Timeout);
}

void NRF24L01_SetRFChannel(NRF24L01_HandleTypeDef *h, uint8_t Channel, uint16_t Timeout)
{
    _write_reg(h, NRF24L01_REG_RF_CH, Channel, Timeout);
}

void NRF24L01_SetRFTxOutputPower(NRF24L01_HandleTypeDef *h, NRF24L01_PWRTypeDef dBm, uint16_t Timeout)
{
    uint8_t regVal = _read_reg(h, NRF24L01_REG_RF_SETUP, Timeout) & NRF24L01_RF_PWR_BIT_MASK_RST;
    regVal |= (uint8_t)(dBm << NRF24L01_RF_PWR);
    _write_reg(h, NRF24L01_REG_RF_SETUP, regVal, Timeout);
}

void NRF24L01_SetRFAirDataRate(NRF24L01_HandleTypeDef *h, NRF24L01_DataRateTypeDef bps, uint16_t Timeout)
{
    uint8_t regVal = _read_reg(h, NRF24L01_REG_RF_SETUP, Timeout) & NRF24L01_RF_DR_BIT_MASK_RST;
    switch (bps)
    {
    case NRF24L01_DATA_RATE_1MBPS: break;
    case NRF24L01_DATA_RATE_2MBPS: regVal |= (1U << 3); break;
    case NRF24L01_DATA_RATE_250KBPS: regVal |= (1U << 5); break;
    default: break;
    }
    _write_reg(h, NRF24L01_REG_RF_SETUP, regVal, Timeout);
}

/* ----------------------------------- IRQ ------------------------------------ */

void NRF24L01_TxIRQHandle(NRF24L01_HandleTypeDef *h, uint16_t Timeout)
{
    uint8_t txDataSent = NRF24L01_GetStatus(h, Timeout) & NRF24L01_TX_DS_MASK;
    if (txDataSent) {
        NRF24L01_ClearTxDS(h, Timeout);
    } else {
        NRF24L01_ClearMaxRT(h, Timeout);
    }
}

/* ---------------------------------- Status ---------------------------------- */

uint8_t NRF24L01_GetStatus(NRF24L01_HandleTypeDef *h, uint16_t Timeout)
{
    (void)Timeout;
    uint8_t cmd    = NRF24L01_CMD_NOP;
    uint8_t status = 0;
#if defined(USE_HAL_DRIVER)
    HAL_GPIO_WritePin(h->CS_GPIO_PORT, h->CS_GPIO_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(h->hspi, &cmd, &status, 1, 2000);
    HAL_GPIO_WritePin(h->CS_GPIO_PORT, h->CS_GPIO_PIN, GPIO_PIN_SET);
#endif
    return status;
}
