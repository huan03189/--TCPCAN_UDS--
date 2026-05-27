#ifndef SRESS_TEST_PLOT_H
#define SRESS_TEST_PLOT_H

#include <QWidget>
#include <QtCharts/QChart>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QChartView>
#include <QTimer>
#include <QMutex>
#include <QLabel>
#include <QPushButton>
#include <QFile>
#include <QTextStream>
#include <QMouseEvent>

class Stress_Test_Plot : public QWidget
{
    Q_OBJECT

public:
    explicit Stress_Test_Plot(QWidget *parent = nullptr);
    ~Stress_Test_Plot();

signals:
    void WindowsClosed();

public slots:
    void updateChartData(qreal busload, qreal packetLossRate);

private slots:
    void onPlotTimer();
    void on_close_btn_clicked();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupUI();
    void createTitleBar();
    void createChartView();
    void addThresholdLines();
    void createLegend();
    void createValueLabels();
    void initDataFile();
    void saveDataPoint(qreal busLoad, qreal packetLossRate);
    void updateThresholdLines(qreal maxX);

    // 拖动
    bool m_dragging = false;
    QPoint m_dragPosition;

    // 标题栏
    QWidget* m_titleBar = nullptr;
    QLabel* m_titleLabel = nullptr;
    QPushButton* m_closeBtn = nullptr;

    // 图表
    QChartView* m_chartView = nullptr;
    QChart* m_chart = nullptr;
    QSplineSeries* m_busLoadSeries = nullptr;
    QSplineSeries* m_packetLossSeries = nullptr;
    QValueAxis* m_axisX = nullptr;
    QValueAxis* m_axisY = nullptr;

    // 警戒线
    QLineSeries* m_loadThresholdSeries = nullptr;
    QLineSeries* m_lossThresholdSeries = nullptr;

    // 图例和数值（浮动在图表上方）
    QLabel* m_legendLoad = nullptr;
    QLabel* m_legendLoss = nullptr;
    QLabel* m_valueLoad = nullptr;
    QLabel* m_valueLoss = nullptr;

    // 定时器
    QTimer* m_plotTimer = nullptr;
    qreal m_timeCounter = 0;

    // 数据缓冲
    struct DataPoint {
        qreal busload;
        qreal packetlossRate;
    };
    QList<DataPoint> m_buffer;
    QMutex m_bufferMutex;

    // 文件
    QFile m_dataFile;
    QTextStream m_dataStream;
};

#endif
