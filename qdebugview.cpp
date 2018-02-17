#include "stdafx.h"
#include <QtGui>
#include <QStyleOptionFocusRect>
#include "qdebugview.h"
#include "Emulator.h"
#include "emubase/Emubase.h"


//////////////////////////////////////////////////////////////////////


QDebugView::QDebugView(QWidget *parent) :
    QWidget(parent)
{
    QFont font = Common_GetMonospacedFont();
    QFontMetrics fontmetrics(font);
    int cxChar = fontmetrics.averageCharWidth();
    int cyLine = fontmetrics.height();
    this->setMinimumSize(cxChar * 55, cyLine * 14 + cyLine / 2);
    this->setMaximumHeight(cyLine * 14 + cyLine / 2);

    setFocusPolicy(Qt::ClickFocus);

    memset(m_wDebugCpuR, 255, sizeof(m_wDebugCpuR));
    memset(m_okDebugCpuRChanged, 1, sizeof(m_okDebugCpuRChanged));
}

void QDebugView::updateData()
{
    CProcessor* pCPU = g_pBoard->GetCPU();
    ASSERT(pCPU != NULL);

    // Get new register values and set change flags
    for (int r = 0; r < 8; r++)
    {
        quint16 value = pCPU->GetReg(r);
        m_okDebugCpuRChanged[r] = (m_wDebugCpuR[r] != value);
        m_wDebugCpuR[r] = value;
    }
    quint16 pswCPU = pCPU->GetPSW();
    m_okDebugCpuRChanged[8] = (m_wDebugCpuR[8] != pswCPU);
    m_wDebugCpuR[8] = pswCPU;
}

void QDebugView::focusInEvent(QFocusEvent *)
{
    repaint();  // Need to draw focus rect
}
void QDebugView::focusOutEvent(QFocusEvent *)
{
    repaint();  // Need to draw focus rect
}

void QDebugView::paintEvent(QPaintEvent * /*event*/)
{
    if (g_pBoard == NULL) return;

    QPainter painter(this);
    painter.fillRect(0, 0, this->width(), this->height(), Qt::white);

    QFont font = Common_GetMonospacedFont();
    painter.setFont(font);
    QFontMetrics fontmetrics(font);
    int cxChar = fontmetrics.averageCharWidth();
    int cyLine = fontmetrics.height();

    CProcessor* pDebugPU = g_pBoard->GetCPU();
    ASSERT(pDebugPU != NULL);
    quint16* arrR = m_wDebugCpuR;
    bool* arrRChanged = m_okDebugCpuRChanged;

    drawProcessor(painter, pDebugPU, cxChar * 2, 1 * cyLine, arrR, arrRChanged);

    // Draw stack
    drawMemoryForRegister(painter, 6, pDebugPU, 35 * cxChar, 1 * cyLine);

    drawPorts(painter, 57 * cxChar, 1 * cyLine);

    // Draw focus rect
    if (hasFocus())
    {
        QStyleOptionFocusRect option;
        option.initFrom(this);
        option.state |= QStyle::State_KeyboardFocusChange;
        option.backgroundColor = QColor(Qt::gray);
        option.rect = this->rect();
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &painter, this);
    }
}

void QDebugView::drawProcessor(QPainter &painter, const CProcessor *pProc, int x, int y, quint16 *arrR, bool *arrRChanged)
{
    QFontMetrics fontmetrics(painter.font());
    int cxChar = fontmetrics.averageCharWidth();
    int cyLine = fontmetrics.height();
    QColor colorText = painter.pen().color();

    painter.setPen(QColor(Qt::gray));
    painter.drawRect(x - cxChar, y - cyLine / 2, 33 * cxChar, cyLine * 13 + cyLine / 2);

    // Registers
    for (int r = 0; r < 8; r++)
    {
        painter.setPen(QColor(arrRChanged[r] ? Qt::red : colorText));

        const char * strRegName = REGISTER_NAME[r];
        painter.drawText(x, y + (1 + r) * cyLine, strRegName);

        quint16 value = arrR[r]; //pProc->GetReg(r);
        DrawOctalValue(painter, x + cxChar * 3, y + (1 + r) * cyLine, value);
        DrawHexValue(painter, x + cxChar * 10, y + (1 + r) * cyLine, value);
        DrawBinaryValue(painter, x + cxChar * 15, y + (1 + r) * cyLine, value);
    }
    painter.setPen(colorText);

    // PSW value
    painter.setPen(QColor(arrRChanged[8] ? Qt::red : colorText));
    painter.drawText(x, y + 10 * cyLine, "PS");
    quint16 psw = arrR[8]; // pProc->GetPSW();
    DrawOctalValue(painter, x + cxChar * 3, y + 10 * cyLine, psw);
    DrawHexValue(painter, x + cxChar * 10, y + 10 * cyLine, psw);
    painter.drawText(x + cxChar * 15, y + 9 * cyLine, "       HP  TNZVC");
    DrawBinaryValue(painter, x + cxChar * 15, y + 10 * cyLine, psw);

    painter.setPen(colorText);

    // Processor mode - HALT or USER
    bool okHaltMode = pProc->IsHaltMode();
    painter.drawText(x, y + 12 * cyLine, okHaltMode ? "HALT" : "USER");

    // "Stopped" flag
    bool okStopped = pProc->IsStopped();
    if (okStopped)
        painter.drawText(x + 6 * cxChar, y + 12 * cyLine, "STOP");
}

void QDebugView::drawMemoryForRegister(QPainter &painter, int reg, CProcessor *pProc, int x, int y)
{
    QFontMetrics fontmetrics(painter.font());
    int cxChar = fontmetrics.averageCharWidth();
    int cyLine = fontmetrics.height();
    QColor colorText = painter.pen().color();

    quint16 current = pProc->GetReg(reg);
    bool okExec = (reg == 7);

    // ������ �� ������ ���������� � �����
    quint16 memory[16];
    for (int idx = 0; idx < 16; idx++)
    {
        int addrtype;
        memory[idx] = g_pBoard->GetWordView(
                current + idx * 2 - 14, pProc->IsHaltMode(), okExec, &addrtype);
    }

    quint16 address = current - 14;
    for (int index = 0; index < 14; index++)    // ������ ������
    {
        // �����
        DrawOctalValue(painter, x + 4 * cxChar, y, address);

        // �������� �� ������
        quint16 value = memory[index];
        quint16 wChanged = Emulator_GetChangeRamStatus(address);
        painter.setPen(wChanged != 0 ? Qt::red : colorText);
        DrawOctalValue(painter, x + 12 * cxChar, y, value);
        painter.setPen(colorText);

        // ������� �������
        if (address == current)
        {
            painter.drawText(x + 2 * cxChar, y, ">>");
            painter.setPen(m_okDebugCpuRChanged[reg] != 0 ? Qt::red : colorText);
            painter.drawText(x, y, REGISTER_NAME[reg]);
            painter.setPen(colorText);
        }

        address += 2;
        y += cyLine;
    }
}

void QDebugView::drawPorts(QPainter &painter, int x, int y)
{
    QFontMetrics fontmetrics(painter.font());
    int cxChar = fontmetrics.averageCharWidth();
    int cyLine = fontmetrics.height();

    painter.drawText(x, y, "Port");

    quint16 value;
    y += cyLine;
    value = g_pBoard->GetPortView(0177660);
    DrawOctalValue(painter, x + 0 * cxChar, y, 0177660);
    DrawOctalValue(painter, x + 8 * cxChar, y, value);
    //DrawBinaryValue(painter, x + 15 * cxChar, y, value);
    painter.drawText(x + 16 * cxChar, y, "keyb state");
    y += cyLine;
    value = g_pBoard->GetPortView(0177662);
    DrawOctalValue(painter, x + 0 * cxChar, y, 0177662);
    DrawOctalValue(painter, x + 8 * cxChar, y, value);
    //DrawBinaryValue(painter, x + 15 * cxChar, y, value);
    painter.drawText(x + 16 * cxChar, y, "keyb data");
    y += cyLine;
    value = g_pBoard->GetPortView(0177664);
    DrawOctalValue(painter, x + 0 * cxChar, y, 0177664);
    DrawOctalValue(painter, x + 8 * cxChar, y, value);
    //DrawBinaryValue(painter, x + 15 * cxChar, y, value);
    painter.drawText(x + 16 * cxChar, y, "scroll");
    y += cyLine;
    value = g_pBoard->GetPortView(0177706);
    DrawOctalValue(painter, x + 0 * cxChar, y, 0177706);
    DrawOctalValue(painter, x + 8 * cxChar, y, value);
    //DrawBinaryValue(painter, x + 15 * cxChar, y, value);
    painter.drawText(x + 16 * cxChar, y, "timer reload");
    y += cyLine;
    value = g_pBoard->GetPortView(0177710);
    DrawOctalValue(painter, x + 0 * cxChar, y, 0177710);
    DrawOctalValue(painter, x + 8 * cxChar, y, value);
    //DrawBinaryValue(painter, x + 15 * cxChar, y, value);
    painter.drawText(x + 16 * cxChar, y, "timer value");
    y += cyLine;
    value = g_pBoard->GetPortView(0177712);
    DrawOctalValue(painter, x + 0 * cxChar, y, 0177712);
    DrawOctalValue(painter, x + 8 * cxChar, y, value);
    //DrawBinaryValue(painter, x + 15 * cxChar, y, value);
    painter.drawText(x + 16 * cxChar, y, "timer manage");
    y += cyLine;
    value = g_pBoard->GetPortView(0177714);
    DrawOctalValue(painter, x + 0 * cxChar, y, 0177714);
    DrawOctalValue(painter, x + 8 * cxChar, y, value);
    //DrawBinaryValue(painter, x + 15 * cxChar, y, value);
    painter.drawText(x + 16 * cxChar, y, "parallel");
    y += cyLine;
    value = g_pBoard->GetPortView(0177716);
    DrawOctalValue(painter, x + 0 * cxChar, y, 0177716);
    DrawOctalValue(painter, x + 8 * cxChar, y, value);
    //DrawBinaryValue(painter, x + 15 * cxChar, y, value);
    painter.drawText(x + 16 * cxChar, y, "system");
    y += cyLine;
    value = g_pBoard->GetPortView(0177130);
    DrawOctalValue(painter, x + 0 * cxChar, y, 0177130);
    DrawOctalValue(painter, x + 8 * cxChar, y, value);
    //DrawBinaryValue(painter, x + 15 * cxChar, y, value);
    painter.drawText(x + 16 * cxChar, y, "floppy state");
    y += cyLine;
    value = g_pBoard->GetPortView(0177132);
    DrawOctalValue(painter, x + 0 * cxChar, y, 0177132);
    DrawOctalValue(painter, x + 8 * cxChar, y, value);
    //DrawBinaryValue(painter, x + 15 * cxChar, y, value);
    painter.drawText(x + 16 * cxChar, y, "floppy data");
}


//////////////////////////////////////////////////////////////////////
