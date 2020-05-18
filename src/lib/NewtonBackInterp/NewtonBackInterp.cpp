#include "NewtonBackInterp.h"

///test values///

void NewtonBackInterp::begin()
{
    this->yvals[0][0] = 46;
    this->yvals[1][0] = 66;
    this->yvals[2][0] = 81;
    this->yvals[3][0] = 93;
    this->yvals[4][0] = 101;

    this->xvals[0] = 1891;
    this->xvals[1] = 1901;
    this->xvals[2] = 1911;
    this->xvals[3] = 1921;
    this->xvals[4] = 1931;

    this->needRecalc = true;
}

void NewtonBackInterp::update(uint32_t newValy, uint32_t newValx)
{

    // this->yvals[0][0] = this->yvals[1][0];
    // this->yvals[1][0] = this->yvals[2][0];
    // this->yvals[2][0] = this->yvals[3][0];
    // this->yvals[3][0] = this->yvals[4][0];
    // this->yvals[4][0] = newValy;

    // this->xvals[0] = this->xvals[1];
    // this->xvals[1] = this->xvals[2];
    // this->xvals[2] = this->xvals[3];
    // this->xvals[3] = this->xvals[4];
    // this->xvals[4] = newValx;

    this->yvals[0][0] = this->yvals[1][0];
    //this->yvals[1][0] = this->yvals[2][0];
    this->yvals[1][0] = newValy;

    this->xvals[0] = this->xvals[1];
    //this->xvals[1] = this->xvals[2];
    this->xvals[1] = newValx;

    this->needRecalc = true;
}

int NewtonBackInterp::fact(int n)
{
    int f = 1;
    for (int i = 2; i <= n; i++)
        f *= i;
    return f;
}

float NewtonBackInterp::calcU(float u, int n)
{
    float temp = u;
    for (int i = 1; i < n; i++)
        temp = temp * (u + i);
    return temp;
}

void NewtonBackInterp::calcBdiffTable()
{

    for (int i = 1; i < polyOrder; i++)
    {
        for (int j = polyOrder - 1; j >= i; j--)
        {
            this->yvals[j][i] = this->yvals[j][i - 1] - this->yvals[j - 1][i - 1];
        }
    }

    this->needRecalc = false;
}

void NewtonBackInterp::printBdiffTable()
{
    for (int i = 0; i < polyOrder; i++)
    {
        for (int j = 0; j <= i; j++)
        {
            Serial.print(this->yvals[i][j]);
        }
        Serial.println("");
    }
}

bool NewtonBackInterp::handle()
{
    if (this->needRecalc)
    {
        this->calcBdiffTable();
        this->needRecalc = false;
        return true;
    }
    else
    {
        return false;
    }
}

void NewtonBackInterp::interp(uint32_t xval)
{
    float sum = this->yvals[polyOrder - 1][0];
    float u = (xval - (this->xvals[polyOrder - 1])) / float((this->xvals[1] - this->xvals[0]));

    for (int i = 1; i < polyOrder; i++)
    {
        sum = sum + (this->calcU(u, i) * float(this->yvals[polyOrder - 1][i])) / float(fact(i));
    }

    this->prediction = sum;
}

uint16_t NewtonBackInterp::getPrediction()
{
    return this->prediction;
}
