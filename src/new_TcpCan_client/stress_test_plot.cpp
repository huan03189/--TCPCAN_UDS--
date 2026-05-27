#include "stress_test_plot.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QDebug>
#include <QPen>
#include <QFont>
#include <QGraphicsDropShadowEffect>


Stress_Test_Plot::Stress_Test_Plot(QWidget *parent)
    : QWidget(parent)
    , m_timeCounter(0)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumSize(800, 500);
    resize(900, 550);
    setWindowTitle("CAN 总线压力测试");

    setupUI();
    createChartView();
    addThresholdLines();
    createLegend();
    createValueLabels();
    initDataFile();

    m_plotTimer = new QTimer(this);
    connect(m_plotTimer, &QTimer::timeout, this, &Stress_Test_Plot::onPlotTimer);
    m_plotTimer->start(50);
}

Stress_Test_Plot::~Stress_Test_Plot()
{
    if (m_plotTimer) m_plotTimer->stop();
    if (m_dataFile.isOpen()) {
        m_dataFile.close();
        qDebug() << "[数据文件] 已保存";
    }
}


void Stress_Test_Plot::setupUI()
{
    // 外层容器
    QWidget* container = new QWidget(this);
    container->setObjectName("container");
    container->setStyleSheet(
        "#container {"
        "   background-color: #121218;"
        "   border-radius: 8px;"
        "}"
        );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(container);

    QVBoxLayout* inner = new QVBoxLayout(container);
    inner->setContentsMargins(0, 0, 0, 0);
    inner->setSpacing(0);

    // 标题栏
    createTitleBar();
    inner->addWidget(m_titleBar);

    // 图表视图（占满剩余空间）
    m_chartView = new QChartView();
    m_chartView->setRenderHint(QPainter::Antialiasing, true);
    m_chartView->setStyleSheet("background: transparent; border: none;");
    inner->addWidget(m_chartView, 1);

    // 阴影
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 100));
    shadow->setOffset(0, 4);
    setGraphicsEffect(shadow);
}


void Stress_Test_Plot::createTitleBar()
{
    m_titleBar = new QWidget();
    m_titleBar->setFixedHeight(36);
    m_titleBar->setStyleSheet(
        "background-color: #1a1a24;"
        "border-top-left-radius: 8px;"
        "border-top-right-radius: 8px;"
        );
    m_titleBar->installEventFilter(this);

    QHBoxLayout* lay = new QHBoxLayout(m_titleBar);
    lay->setContentsMargins(12, 0, 8, 0);
    lay->setSpacing(0);

    m_titleLabel = new QLabel("CAN 总线压力测试");
    m_titleLabel->setStyleSheet(
        "color: #b0b0b0;"
        "font: bold 12px 'Segoe UI', 'Microsoft YaHei';"
        "background: transparent;"
        );
    lay->addWidget(m_titleLabel);
    lay->addStretch();

    m_closeBtn = new QPushButton("✕");
    m_closeBtn->setFixedSize(28, 28);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(
        "QPushButton {"
        "   color: #888; background: transparent; border: none;"
        "   font-size: 14px; border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "   color: #fff; background-color: #e04040;"
        "}"
        );
    connect(m_closeBtn, &QPushButton::clicked, this, &Stress_Test_Plot::on_close_btn_clicked);
    lay->addWidget(m_closeBtn);
}


void Stress_Test_Plot::createChartView()
{
    m_chart = new QChart();
    m_chart->setBackgroundBrush(QColor(18, 18, 24));
    m_chart->setPlotAreaBackgroundBrush(QColor(22, 22, 30));
    m_chart->setPlotAreaBackgroundVisible(true);
    m_chart->setMargins(QMargins(10, 10, 10, 10));
    m_chart->legend()->setVisible(false);

    // 总线负载率
    m_busLoadSeries = new QSplineSeries();
    m_busLoadSeries->setColor(QColor(0, 210, 120));
    m_busLoadSeries->setPen(QPen(QColor(0, 210, 120), 2.5, Qt::SolidLine, Qt::RoundCap));
    m_busLoadSeries->setPointsVisible(false);

    // 丢包率
    m_packetLossSeries = new QSplineSeries();
    m_packetLossSeries->setColor(QColor(255, 170, 40));
    m_packetLossSeries->setPen(QPen(QColor(255, 170, 40), 2.5, Qt::SolidLine, Qt::RoundCap));
    m_packetLossSeries->setPointsVisible(false);

    m_chart->addSeries(m_busLoadSeries);
    m_chart->addSeries(m_packetLossSeries);

    // X 轴
    m_axisX = new QValueAxis();
    m_axisX->setRange(0, 120);
    m_axisX->setTickCount(7);
    m_axisX->setLabelFormat("%.0f s");
    m_axisX->setGridLineColor(QColor(40, 40, 50));
    m_axisX->setLabelsColor(QColor(140, 140, 150));

    // Y 轴
    m_axisY = new QValueAxis();
    m_axisY->setRange(0, 100);
    m_axisY->setTickCount(6);
    m_axisY->setLabelFormat("%.0f%%");
    m_axisY->setGridLineColor(QColor(40, 40, 50));
    m_axisY->setLabelsColor(QColor(140, 140, 150));

    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);

    m_busLoadSeries->attachAxis(m_axisX);
    m_busLoadSeries->attachAxis(m_axisY);
    m_packetLossSeries->attachAxis(m_axisX);
    m_packetLossSeries->attachAxis(m_axisY);

    m_chartView->setChart(m_chart);
}


void Stress_Test_Plot::addThresholdLines()
{
    m_loadThresholdSeries = new QLineSeries();
    m_loadThresholdSeries->setPen(QPen(QColor(255, 60, 60), 1, Qt::DashLine));
    m_loadThresholdSeries->append(0, 80);
    m_loadThresholdSeries->append(10, 80);

    m_lossThresholdSeries = new QLineSeries();
    m_lossThresholdSeries->setPen(QPen(QColor(255, 140, 40), 1, Qt::DashLine));
    m_lossThresholdSeries->append(0, 3);
    m_lossThresholdSeries->append(10, 3);

    m_chart->addSeries(m_loadThresholdSeries);
    m_chart->addSeries(m_lossThresholdSeries);

    m_loadThresholdSeries->attachAxis(m_axisX);
    m_loadThresholdSeries->attachAxis(m_axisY);
    m_lossThresholdSeries->attachAxis(m_axisX);
    m_lossThresholdSeries->attachAxis(m_axisY);
}

void Stress_Test_Plot::updateThresholdLines(qreal maxX)
{
    if (!m_loadThresholdSeries || !m_lossThresholdSeries) return;
    m_loadThresholdSeries->replace(0, 0, 80);
    m_loadThresholdSeries->replace(1, maxX, 80);
    m_lossThresholdSeries->replace(0, 0, 3);
    m_lossThresholdSeries->replace(1, maxX, 3);
}


void Stress_Test_Plot::createLegend()
{
    m_legendLoad = new QLabel(this);
    m_legendLoad->setText("● 总线负载率");
    m_legendLoad->setStyleSheet(
        "color: rgb(0, 210, 120); font: bold 11px Consolas;"
        "background: rgba(18, 18, 24, 170); padding: 4px 10px; border-radius: 3px;"
        );
    m_legendLoad->adjustSize();
    m_legendLoad->move(18, 48);

    m_legendLoss = new QLabel(this);
    m_legendLoss->setText("● 丢包率");
    m_legendLoss->setStyleSheet(
        "color: rgb(255, 170, 40); font: bold 11px Consolas;"
        "background: rgba(18, 18, 24, 170); padding: 4px 10px; border-radius: 3px;"
        );
    m_legendLoss->adjustSize();
    m_legendLoss->move(18, 78);
}


void Stress_Test_Plot::createValueLabels()
{
    m_valueLoad = new QLabel(this);
    m_valueLoad->setText("--.-%");
    m_valueLoad->setStyleSheet(
        "color: rgb(0, 210, 120); font: bold 32px Consolas; background: transparent;"
        );
    m_valueLoad->adjustSize();
    m_valueLoad->move(18, 110);

    m_valueLoss = new QLabel(this);
    m_valueLoss->setText("--.-%");
    m_valueLoss->setStyleSheet(
        "color: rgb(255, 170, 40); font: bold 32px Consolas; background: transparent;"
        );
    m_valueLoss->adjustSize();
    m_valueLoss->move(18, 152);
}


void Stress_Test_Plot::initDataFile()
{
    QString fileName = QString("stress_test_%1.csv")
    .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    m_dataFile.setFileName(fileName);
    if (m_dataFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_dataStream.setDevice(&m_dataFile);
        m_dataStream << "序号,总线负载率(%),丢包率(%)\n";
        qDebug() << "[数据文件] 已创建:" << fileName;
    }
}

void Stress_Test_Plot::saveDataPoint(qreal busLoad, qreal packetLossRate)
{
    if (m_dataFile.isOpen()) {
        m_dataStream << (int)m_timeCounter << "," << busLoad << "," << packetLossRate << "\n";
    }
}


bool Stress_Test_Plot::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_titleBar) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* e = static_cast<QMouseEvent*>(event);
            if (e->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragPosition = e->globalPos() - frameGeometry().topLeft();
                return true;
            }
        }
        else if (event->type() == QEvent::MouseMove) {
            QMouseEvent* e = static_cast<QMouseEvent*>(event);
            if (m_dragging && (e->buttons() & Qt::LeftButton)) {
                move(e->globalPos() - m_dragPosition);
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease) {
            m_dragging = false;
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}


void Stress_Test_Plot::on_close_btn_clicked()
{
    qDebug() << "[压力测试窗口] 关闭按钮被点击";
    if (m_plotTimer) m_plotTimer->stop();
    emit WindowsClosed();
    this->hide();
}

void Stress_Test_Plot::updateChartData(qreal busLoad, qreal packetLossRate)
{
    QMutexLocker locker(&m_bufferMutex);
    m_buffer.append({busLoad, packetLossRate});
    saveDataPoint(busLoad, packetLossRate);
}

void Stress_Test_Plot::onPlotTimer()
{
    QList<QPointF> loadPoints;
    QList<QPointF> lossPoints;

    {
        QMutexLocker locker(&m_bufferMutex);
        if (m_buffer.isEmpty()) return;

        for (const auto& pt : m_buffer) {
            loadPoints.append(QPointF(m_timeCounter, pt.busload));
            lossPoints.append(QPointF(m_timeCounter, pt.packetlossRate));
            m_timeCounter++;
        }
        m_buffer.clear();
    }

    for (int i = 0; i < loadPoints.size(); ++i) {
        m_busLoadSeries->append(loadPoints[i]);
        m_packetLossSeries->append(lossPoints[i]);
    }

    // 保留最近 300 个点（5 分钟，每秒 1 点）
    int maxPoints = 300;
    while (m_busLoadSeries->count() > maxPoints) {
        m_busLoadSeries->remove(0);
        m_packetLossSeries->remove(0);
    }

    // 滚动 X 轴
    if (m_timeCounter > maxPoints) {
        m_axisX->setRange(m_timeCounter - maxPoints, m_timeCounter);
    } else {
        m_axisX->setRange(0, maxPoints);
    }

    updateThresholdLines(m_axisX->max());

    // 更新实时数值
    if (m_busLoadSeries->count() > 0) {
        qreal lastLoad = m_busLoadSeries->at(m_busLoadSeries->count() - 1).y();
        m_valueLoad->setText(QString("%1%").arg(lastLoad, 0, 'f', 1));
    }
    if (m_packetLossSeries->count() > 0) {
        qreal lastLoss = m_packetLossSeries->at(m_packetLossSeries->count() - 1).y();
        m_valueLoss->setText(QString("%1%").arg(lastLoss, 0, 'f', 1));
    }
}
