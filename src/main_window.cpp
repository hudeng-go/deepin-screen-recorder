#include <QApplication>
#include <QTimer>
#include <QMouseEvent>
#include <QEvent>
#include <QKeyEvent>
#include <QRegion>
#include <QIcon>
#include <QObject>
#include <QPainter>
#include <QDebug>
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <QWidget>
#include <QThread>
#include "main_window.h"
#include <X11/extensions/shape.h>
#include <QtX11Extras/QX11Info>
#include "utils.h"

MainWindow::MainWindow(QWidget *parent) : QWidget(parent)
{
    // Init attributes.
    setMouseTracking(true);   // make MouseMove can response
    installEventFilter(this);  // add event filter

    firstDrag = false;
    firstMove = false;
    firstPressButton = false;
    firstReleaseButton = false;
    dragStartX = 0;
    dragStartY = 0;

    isPressButton = false;
    isReleaseButton = false;

    moveResizeByKey = false;

    recordX = 0;
    recordY = 0;
    recordWidth = 0;
    recordHeight = 0;

    Settings settings;

    saveAsGif = settings.option("save_as_gif").toBool();

    drawDragPoint = false;

    recordButtonStatus = RECORD_BUTTON_NORMAL;

    countdownCounter = 0;
    flashCounter = 0;

    selectAreaName = "";

    resizeHandleBigImg = QImage(Utils::getImagePath("resize_handle_big.png"));
    resizeHandleSmallImg = QImage(Utils::getImagePath("resize_handle_small.png"));
    countdown1Img = QImage(Utils::getImagePath("countdown_1.png"));
    countdown2Img = QImage(Utils::getImagePath("countdown_2.png"));
    countdown3Img = QImage(Utils::getImagePath("countdown_3.png"));
    recordIconNormalImg = QImage(Utils::getImagePath("record_icon_normal.png"));
    recordIconHoverImg = QImage(Utils::getImagePath("record_icon_hover.png"));
    recordIconPressImg = QImage(Utils::getImagePath("record_icon_press.png"));

    recordGifNormalImg = QImage(Utils::getImagePath("gif_normal.png"));
    recordGifPressImg = QImage(Utils::getImagePath("gif_press.png"));
    recordGifCheckedImg = QImage(Utils::getImagePath("gif_checked.png"));

    recordMp4NormalImg = QImage(Utils::getImagePath("mp4_normal.png"));
    recordMp4PressImg = QImage(Utils::getImagePath("mp4_press.png"));
    recordMp4CheckedImg = QImage(Utils::getImagePath("mp4_checked.png"));

    // Get all windows geometry.
    windowManager = new WindowManager();
    QList<xcb_window_t> windows = windowManager->getWindows();
    rootWindowRect = windowManager->getRootWindowRect();

    for (int i = 0; i < windows.length(); i++) {
        windowRects.append(windowManager->getWindowRect(windows[i]));
        windowNames.append(windowManager->getWindowName(windows[i]));
    }

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon((Utils::getImagePath("trayicon1.svg"))));
    trayIcon->setToolTip("停止录制");
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

    setDragCursor();
}

void MainWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    if (!firstPressButton) {
        QString tooltipString = "点击录制窗口或者全屏\n拖动鼠标选择录制区域";
        setFontSize(painter, 11);
        QSize size = getRenderSize(painter, tooltipString);
        int rectWidth = size.width() + INIT_TOOLTIP_PADDING_X * 2;
        int rectHeight = size.height() + INIT_TOOLTIP_PADDING_Y * 2;
        QRectF tooltipRect((rootWindowRect.width - rectWidth) / 2,
                           (rootWindowRect.height - rectHeight) / 2,
                           rectWidth,
                           rectHeight);

        QList<QRectF> rects;
        rects.append(tooltipRect);
        renderTooltipRect(painter, rects, 0.6);

        painter.setPen(QPen(QColor("#000000")));
        painter.drawText(tooltipRect, Qt::AlignCenter, tooltipString);
    } else {
        clearTooltip();
    }

    if (recordWidth > 0 && recordHeight > 0) {

        QRect backgroundRect = QRect(rootWindowRect.x, rootWindowRect.y, rootWindowRect.width, rootWindowRect.height);
        QRect frameRect = QRect(recordX, recordY, recordWidth, recordHeight);

        // Draw background.
        painter.setBrush(QBrush("#000000"));
        painter.setOpacity(0.2);

        painter.setClipping(true);
        painter.setClipRegion(QRegion(backgroundRect).subtracted(QRegion(frameRect)));
        painter.drawRect(backgroundRect);

        // Draw frame.
        QPen framePen(QColor("#01bdff"));
        framePen.setWidth(2);
        painter.setBrush(QBrush());  // clear brush
        painter.setPen(framePen);
        painter.drawRect(frameRect);

        // Reset clip.
        painter.setClipRegion(QRegion(backgroundRect));

        // Draw drag pint.
        if (recordButtonStatus == RECORD_BUTTON_NORMAL && drawDragPoint) {
            painter.drawImage(QPoint(recordX - DRAG_POINT_RADIUS, recordY - DRAG_POINT_RADIUS), resizeHandleBigImg);
            painter.drawImage(QPoint(recordX - DRAG_POINT_RADIUS + recordWidth, recordY - DRAG_POINT_RADIUS), resizeHandleBigImg);
            painter.drawImage(QPoint(recordX - DRAG_POINT_RADIUS, recordY - DRAG_POINT_RADIUS + recordHeight), resizeHandleBigImg);
            painter.drawImage(QPoint(recordX - DRAG_POINT_RADIUS + recordWidth, recordY - DRAG_POINT_RADIUS + recordHeight), resizeHandleBigImg);
            painter.drawImage(QPoint(recordX - DRAG_POINT_RADIUS, recordY - DRAG_POINT_RADIUS + recordHeight / 2), resizeHandleBigImg);
            painter.drawImage(QPoint(recordX - DRAG_POINT_RADIUS + recordWidth, recordY - DRAG_POINT_RADIUS + recordHeight / 2), resizeHandleBigImg);
            painter.drawImage(QPoint(recordX - DRAG_POINT_RADIUS + recordWidth / 2, recordY - DRAG_POINT_RADIUS), resizeHandleBigImg);
            painter.drawImage(QPoint(recordX - DRAG_POINT_RADIUS + recordWidth / 2, recordY - DRAG_POINT_RADIUS + recordHeight), resizeHandleBigImg);

            painter.drawImage(QPoint(recordX - DRAG_POINT_RADIUS, recordY - DRAG_POINT_RADIUS), resizeHandleSmallImg);
            painter.drawImage(QPoint(recordX - DRAG_POINT_RADIUS + recordWidth, recordY - DRAG_POINT_RADIUS), resizeHandleSmallImg);
            painter.drawImage(QPoint(recordX - DRAG_POINT_RADIUS, recordY - DRAG_POINT_RADIUS + recordHeight), resizeHandleSmallImg);
            painter.drawImage(QPoint(recordX - DRAG_POINT_RADIUS + recordWidth, recordY - DRAG_POINT_RADIUS + recordHeight), resizeHandleSmallImg);
            painter.drawImage(QPoint(recordX - DRAG_POINT_RADIUS, recordY - DRAG_POINT_RADIUS + recordHeight / 2), resizeHandleSmallImg);
            painter.drawImage(QPoint(recordX - DRAG_POINT_RADIUS + recordWidth, recordY - DRAG_POINT_RADIUS + recordHeight / 2), resizeHandleSmallImg);
            painter.drawImage(QPoint(recordX - DRAG_POINT_RADIUS + recordWidth / 2, recordY - DRAG_POINT_RADIUS), resizeHandleSmallImg);
            painter.drawImage(QPoint(recordX - DRAG_POINT_RADIUS + recordWidth / 2, recordY - DRAG_POINT_RADIUS + recordHeight), resizeHandleSmallImg);
        }

        // Draw record panel.
        if (firstPressButton) {
            if (firstReleaseButton) {
                QString buttonString;
                if (recordButtonStatus == RECORD_BUTTON_NORMAL) {
                    if (isReleaseButton && !moveResizeByKey) {
                        int recordButtonX = recordX + (recordWidth - RECORD_BUTTON_AREA_WIDTH) / 2;
                        int recordButtonY = recordY + (recordHeight - RECORD_BUTTON_AREA_HEIGHT - RECORD_OPTIONS_AREA_HEIGHT - RECORD_OPTIONS_AREA_PADDING) / 2;
                        QRectF recordButtonRect(recordButtonX, recordButtonY, RECORD_BUTTON_AREA_WIDTH, RECORD_BUTTON_AREA_HEIGHT);

                        int recordOptionsX = recordX + (recordWidth - RECORD_BUTTON_AREA_WIDTH) / 2;
                        int recordOptionsY = recordY + (recordHeight + RECORD_BUTTON_AREA_HEIGHT - RECORD_OPTIONS_AREA_HEIGHT + RECORD_OPTIONS_AREA_PADDING) / 2;
                        QRectF recordOptionsRect(recordOptionsX, recordOptionsY, RECORD_BUTTON_AREA_WIDTH, RECORD_OPTIONS_AREA_HEIGHT);

                        QList<QRectF> rects;
                        rects.append(recordButtonRect);
                        rects.append(recordOptionsRect);

                        if (recordButtonState == BUTTON_STATE_NORMAL) {
                            renderTooltipRect(painter, rects, 0.6);
                            painter.drawImage(QPoint(recordX + (recordWidth - recordIconNormalImg.width()) / 2, recordButtonY + RECORD_BUTTON_OFFSET_Y), recordIconNormalImg);
                        } else if (recordButtonState == BUTTON_STATE_HOVER) {
                            renderTooltipRect(painter, rects, 0.7);
                            painter.drawImage(QPoint(recordX + (recordWidth - recordIconNormalImg.width()) / 2, recordButtonY + RECORD_BUTTON_OFFSET_Y), recordIconHoverImg);
                        } else if (recordButtonState == BUTTON_STATE_PRESS) {
                            renderTooltipRect(painter, rects, 0.2);
                            painter.drawImage(QPoint(recordX + (recordWidth - recordIconNormalImg.width()) / 2, recordButtonY + RECORD_BUTTON_OFFSET_Y), recordIconPressImg);
                        }

                        QRectF recordStringRect(recordButtonX,
                                                recordButtonY + recordIconNormalImg.height(),
                                                RECORD_BUTTON_AREA_WIDTH,
                                                RECORD_BUTTON_AREA_HEIGHT - recordIconNormalImg.height());
                        QString recordString = "开始录制";
                        setFontSize(painter, 11);
                        painter.setPen(QPen(QColor("#e34342")));
                        painter.drawText(recordStringRect, Qt::AlignCenter, recordString);

                        int recordOptionGifX = recordX + (recordWidth - RECORD_BUTTON_AREA_WIDTH) / 2 + BUTTON_OPTION_ICON_OFFSET_X;
                        int recordOptionMp4X = recordX + (recordWidth - RECORD_BUTTON_AREA_WIDTH) / 2 + RECORD_BUTTON_AREA_WIDTH / 2;
                        int recordOptionY = recordOptionsY + (RECORD_OPTIONS_AREA_HEIGHT - BUTTON_OPTION_HEIGHT) / 2 ;
                        if (recordOptionGifState == BUTTON_STATE_PRESS) {
                            painter.drawImage(QPoint(recordOptionGifX, recordOptionY), recordGifPressImg);
                        } else if (saveAsGif) {
                            painter.drawImage(QPoint(recordOptionGifX, recordOptionY), recordGifCheckedImg);
                        } else {
                            painter.drawImage(QPoint(recordOptionGifX, recordOptionY), recordGifNormalImg);
                        }
                        QString optionGifString = "GIF";
                        setFontSize(painter, 9);
                        if (saveAsGif) {
                            painter.setPen(QPen(QColor("#2ca7f8")));
                        } else {
                            painter.setPen(QPen(QColor("#000000")));
                        }
                        painter.drawText(QRectF(recordOptionGifX + recordGifNormalImg.width(),
                                                recordOptionsY,
                                                RECORD_BUTTON_AREA_WIDTH / 2 - BUTTON_OPTION_ICON_OFFSET_X - recordGifNormalImg.width(),
                                                RECORD_OPTIONS_AREA_HEIGHT),
                                         Qt::AlignVCenter,
                                         optionGifString);

                        if (recordOptionMp4State == BUTTON_STATE_PRESS) {
                            painter.drawImage(QPoint(recordOptionMp4X, recordOptionY), recordMp4PressImg);
                        } else if (!saveAsGif) {
                            painter.drawImage(QPoint(recordOptionMp4X, recordOptionY), recordMp4CheckedImg);
                        } else {
                            painter.drawImage(QPoint(recordOptionMp4X, recordOptionY), recordMp4NormalImg);
                        }
                        QString optionMp4String = "MP4";
                        setFontSize(painter, 9);
                        if (saveAsGif) {
                            painter.setPen(QPen(QColor("#000000")));
                        } else {
                            painter.setPen(QPen(QColor("#2ca7f8")));
                        }
                        painter.drawText(QRectF(recordOptionMp4X + recordMp4NormalImg.width(),
                                                recordOptionsY,
                                                RECORD_BUTTON_AREA_WIDTH / 2 - BUTTON_OPTION_ICON_OFFSET_X - recordMp4NormalImg.width(),
                                                RECORD_OPTIONS_AREA_HEIGHT),
                                         Qt::AlignVCenter,
                                         optionMp4String);
                    } else {
                        clearTooltip();
                    }
                }

                painter.setClipping(false);

                // Draw record wait second.
                if (countdownCounter > 0) {
                    QString tooltipString = "停止录制请点击托盘图标\n或着\n按下深度录屏快捷键";
                    setFontSize(painter, 11);
                    QSize size = getRenderSize(painter, tooltipString);
                    int tooltipWidth = size.width() + COUNTDOWN_TOOLTIP_PADDING_X * 2;
                    int tooltipHeight = size.height() + COUNTDOWN_TOOLTIP_PADDING_Y * 2;
                    int rectWidth = tooltipWidth;
                    int rectHeight = tooltipHeight + countdown1Img.height() + COUNTDOWN_TOOLTIP_NUMBER_PADDING_Y;

                    QRectF countdownRect(recordX + (recordWidth - rectWidth) / 2,
                                         recordY + (recordHeight - rectHeight) / 2,
                                         rectWidth,
                                         rectHeight);

                    QList<QRectF> rects;
                    rects.append(countdownRect);
                    renderTooltipRect(painter, rects, 0.6);

                    int countdownX = recordX + (recordWidth - countdown1Img.width()) / 2;
                    int countdownY = recordY + (recordHeight - rectHeight) / 2 + COUNTDOWN_TOOLTIP_NUMBER_PADDING_Y;

                    if (countdownCounter == 1) {
                        painter.drawImage(QPoint(countdownX, countdownY), countdown1Img);
                    } else if (countdownCounter == 2) {
                        painter.drawImage(QPoint(countdownX, countdownY), countdown2Img);
                    } else if (countdownCounter == 3) {
                        painter.drawImage(QPoint(countdownX, countdownY), countdown3Img);
                    }

                    QRectF tooltipRect(recordX + (recordWidth - rectWidth) / 2,
                                       recordY + (recordHeight - rectHeight) / 2 + countdown1Img.height() + COUNTDOWN_TOOLTIP_NUMBER_PADDING_Y,
                                       rectWidth,
                                       tooltipHeight);
                    painter.setPen(QPen(QColor("#000000")));
                    painter.drawText(tooltipRect, Qt::AlignCenter, tooltipString);
                }
            }
        }
    }
}

bool MainWindow::eventFilter(QObject *, QEvent *event)
{
#undef KeyPress
#undef KeyRelease
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        if (keyEvent->key() == Qt::Key_Escape) {
            if (recordButtonStatus != RECORD_BUTTON_RECORDING) {
                QApplication::quit();
            }
        }

        if (recordButtonStatus == RECORD_BUTTON_NORMAL) {
            if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
                if (keyEvent->key() == Qt::Key_Left) {
                    recordX = std::max(0, recordX - 1);
                    recordWidth = std::min(recordWidth + 1, rootWindowRect.width);

                    moveResizeByKey = true;

                    repaint();
                } else if (keyEvent->key() == Qt::Key_Right) {
                    recordWidth = std::min(recordWidth + 1, rootWindowRect.width);

                    moveResizeByKey = true;

                    repaint();
                } else if (keyEvent->key() == Qt::Key_Up) {
                    recordY = std::max(0, recordY - 1);
                    recordHeight = std::min(recordHeight + 1, rootWindowRect.height);

                    moveResizeByKey = true;

                    repaint();
                } else if (keyEvent->key() == Qt::Key_Down) {
                    recordHeight = std::min(recordHeight + 1, rootWindowRect.height);

                    moveResizeByKey = true;

                    repaint();
                }
            } else {
                if (keyEvent->key() == Qt::Key_Left) {
                    recordX = std::max(0, recordX - 1);

                    moveResizeByKey = true;

                    repaint();
                } else if (keyEvent->key() == Qt::Key_Right) {
                    recordX = std::min(rootWindowRect.width - recordWidth, recordX + 1);

                    moveResizeByKey = true;

                    repaint();
                } else if (keyEvent->key() == Qt::Key_Up) {
                    recordY = std::max(0, recordY - 1);

                    moveResizeByKey = true;

                    repaint();
                } else if (keyEvent->key() == Qt::Key_Down) {
                    recordY = std::min(rootWindowRect.height - recordHeight, recordY + 1);

                    moveResizeByKey = true;

                    repaint();
                }
            }
        }
    } else if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        // NOTE: must be use 'isAutoRepeat' to filter KeyRelease event send by Qt.
        if (!keyEvent->isAutoRepeat()) {
            if (keyEvent->key() == Qt::Key_Left || keyEvent->key() == Qt::Key_Right || keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) {
                moveResizeByKey = false;
                repaint();
            }
        }

    }

    int recordButtonX = recordX + (recordWidth - RECORD_BUTTON_AREA_WIDTH) / 2;
    int recordButtonY = recordY + (recordHeight - RECORD_BUTTON_AREA_HEIGHT - RECORD_OPTIONS_AREA_HEIGHT - RECORD_OPTIONS_AREA_PADDING) / 2;

    int recordOptionGifX = recordX + (recordWidth - RECORD_BUTTON_AREA_WIDTH) / 2;
    int recordOptionMp4X = recordX + (recordWidth - RECORD_BUTTON_AREA_WIDTH) / 2 + RECORD_BUTTON_AREA_WIDTH / 2;
    int recordOptionY = recordY + (recordHeight + RECORD_BUTTON_AREA_HEIGHT - RECORD_OPTIONS_AREA_HEIGHT + RECORD_OPTIONS_AREA_PADDING) / 2;

    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        dragStartX = mouseEvent->x();
        dragStartY = mouseEvent->y();
        if (!firstPressButton) {
            firstPressButton = true;
        } else {
            dragAction = getAction(event);

            dragRecordX = recordX;
            dragRecordY = recordY;
            dragRecordWidth = recordWidth;
            dragRecordHeight = recordHeight;

            if (firstReleaseButton) {
                int pressX = mouseEvent->x();
                int pressY = mouseEvent->y();

                if (pressX > recordButtonX && pressX < recordButtonX + RECORD_BUTTON_AREA_WIDTH && pressY > recordButtonY && pressY < recordButtonY + RECORD_BUTTON_AREA_HEIGHT) {
                    recordButtonState = BUTTON_STATE_PRESS;

                    if (recordButtonStatus == RECORD_BUTTON_NORMAL) {
                        recordButtonStatus = RECORD_BUTTON_WAIT;

                        countdownCounter = 3;
                        showCountdownTimer = new QTimer(this);
                        connect(showCountdownTimer, SIGNAL(timeout()), this, SLOT(showCountdown()));
                        showCountdownTimer->start(1000);

                        recordProcess.setRecordInfo(recordX, recordY, recordWidth, recordHeight, selectAreaName);
                        if (saveAsGif) {
                            recordProcess.setRecordType(RecordProcess::RECORD_TYPE_GIF);
                        } else {
                            recordProcess.setRecordType(RecordProcess::RECORD_TYPE_VIDEO);
                        }

                        resetCursor();

                        repaint();
                    }
                } else if (pressX > recordOptionGifX && pressX < recordOptionGifX + RECORD_BUTTON_AREA_WIDTH / 2
                           && pressY > recordOptionY && pressY < recordOptionY + RECORD_OPTIONS_AREA_HEIGHT) {
                    if (recordButtonStatus == RECORD_BUTTON_NORMAL) {
                        recordOptionGifState = BUTTON_STATE_PRESS;
                        recordOptionMp4State = BUTTON_STATE_NORMAL;

                        saveAsGif = true;
                        settings.setOption("save_as_gif", saveAsGif);

                        repaint();
                    }
                } else if (pressX > recordOptionMp4X && pressX < recordOptionMp4X + RECORD_BUTTON_AREA_WIDTH / 2
                           && pressY > recordOptionY && pressY < recordOptionY + RECORD_OPTIONS_AREA_HEIGHT) {
                    if (recordButtonStatus == RECORD_BUTTON_NORMAL) {
                        recordOptionMp4State = BUTTON_STATE_PRESS;
                        recordOptionGifState = BUTTON_STATE_NORMAL;

                        saveAsGif = false;
                        settings.setOption("save_as_gif", saveAsGif);

                        repaint();
                    }
                }
            }
        }

        isPressButton = true;
        isReleaseButton = false;
    } else if (event->type() == QEvent::MouseButtonRelease) {
        if (!firstReleaseButton) {
            firstReleaseButton = true;

            updateCursor(event);

            // Record select area name with window name if just click (no drag).
            if (!firstDrag) {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
                for (int i = 0; i < windowRects.length(); i++) {
                    int wx = windowRects[i].x;
                    int wy = windowRects[i].y;
                    int ww = windowRects[i].width;
                    int wh = windowRects[i].height;
                    int ex = mouseEvent->x();
                    int ey = mouseEvent->y();
                    if (ex > wx && ex < wx + ww && ey > wy && ey < wy + wh) {
                        selectAreaName = windowNames[i];

                        break;
                    }
                }

            }
        }

        isPressButton = false;
        isReleaseButton = true;

        repaint();
    } else if (event->type() == QEvent::MouseMove) {
        if (!firstMove) {
            firstMove = true;
        }

        if (isPressButton && firstPressButton) {
            if (!firstDrag) {
                firstDrag = true;

                selectAreaName = "自由选区";
            }
        }

        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        int pressX = mouseEvent->x();
        int pressY = mouseEvent->y();

        if (pressX > recordButtonX && pressX < recordButtonX + RECORD_BUTTON_AREA_WIDTH && pressY > recordButtonY && pressY < recordButtonY + RECORD_BUTTON_AREA_HEIGHT) {
            recordButtonState = BUTTON_STATE_HOVER;

            repaint();
        } else {
            recordButtonState = BUTTON_STATE_NORMAL;

            repaint();
        }

        if (firstPressButton) {
            if (!firstReleaseButton) {
                if (isPressButton && !isReleaseButton) {
                    recordX = std::min(dragStartX, mouseEvent->x());
                    recordY = std::min(dragStartY, mouseEvent->y());
                    recordWidth = std::abs(dragStartX - mouseEvent->x());
                    recordHeight = std::abs(dragStartY - mouseEvent->y());

                    repaint();
                }
            } else if (isPressButton) {
                if (recordButtonStatus == RECORD_BUTTON_NORMAL) {
                    if (dragAction != ACTION_STAY) {
                        selectAreaName = "自由选区";
                    }

                    if (dragAction == ACTION_MOVE) {
                        recordX = std::max(std::min(dragRecordX + mouseEvent->x() - dragStartX, rootWindowRect.width - recordWidth), 1);
                        recordY = std::max(std::min(dragRecordY + mouseEvent->y() - dragStartY, rootWindowRect.height - recordHeight), 1);
                    } else if (dragAction == ACTION_RESIZE_TOP_LEFT) {
                        resizeTop(mouseEvent);
                        resizeLeft(mouseEvent);
                    } else if (dragAction == ACTION_RESIZE_TOP_RIGHT) {
                        resizeTop(mouseEvent);
                        resizeRight(mouseEvent);
                    } else if (dragAction == ACTION_RESIZE_BOTTOM_LEFT) {
                        resizeBottom(mouseEvent);
                        resizeLeft(mouseEvent);
                    } else if (dragAction == ACTION_RESIZE_BOTTOM_RIGHT) {
                        resizeBottom(mouseEvent);
                        resizeRight(mouseEvent);
                    } else if (dragAction == ACTION_RESIZE_TOP) {
                        resizeTop(mouseEvent);
                    } else if (dragAction == ACTION_RESIZE_BOTTOM) {
                        resizeBottom(mouseEvent);
                    } else if (dragAction == ACTION_RESIZE_LEFT) {
                        resizeLeft(mouseEvent);
                    } else if (dragAction == ACTION_RESIZE_RIGHT) {
                        resizeRight(mouseEvent);
                    }

                    repaint();
                }
            }

            updateCursor(event);

            int action = getAction(event);
            bool drawPoint = action != ACTION_MOVE && action != ACTION_STAY;
            if (drawPoint != drawDragPoint) {
                drawDragPoint = drawPoint;
                repaint();
            }
        } else {
            for (int i = 0; i < windowRects.length(); i++) {
                int wx = windowRects[i].x;
                int wy = windowRects[i].y;
                int ww = windowRects[i].width;
                int wh = windowRects[i].height;
                int ex = mouseEvent->x();
                int ey = mouseEvent->y();
                if (ex > wx && ex < wx + ww && ey > wy && ey < wy + wh) {
                    recordX = wx;
                    recordY = wy;
                    recordWidth = ww;
                    recordHeight = wh;

                    repaint();

                    break;
                }
            }
        }
    }

    return false;
}

void MainWindow::showCountdown()
{
    countdownCounter--;
    repaint();

    if (countdownCounter <= 0) {
        showCountdownTimer->stop();

        clearTooltip();

        recordButtonStatus = RECORD_BUTTON_RECORDING;
        repaint();

        updateMouseEventArea();

        resetCursor();

        recordProcess.start();

        trayIcon->show();

        flashTryIconTimer = new QTimer(this);
        connect(flashTryIconTimer, SIGNAL(timeout()), this, SLOT(flashTrayIcon()));
        flashTryIconTimer->start(500);
    }
}

void MainWindow::flashTrayIcon()
{
    if (flashCounter % 2 == 0) {
        trayIcon->setIcon(QIcon((Utils::getImagePath("trayicon2.svg"))));
    } else {
        trayIcon->setIcon(QIcon((Utils::getImagePath("trayicon1.svg"))));
    }

    flashCounter++;

    if (flashCounter > 10) {
        flashCounter = 1;
    }
}

void MainWindow::updateMouseEventArea()
{
    XRectangle* reponseArea = new XRectangle;
    reponseArea->x = 0;
    reponseArea->y = 0;
    reponseArea->width = 0;
    reponseArea->height = 0;

    XShapeCombineRectangles(QX11Info::display(), winId(), ShapeInput, 0, 0, reponseArea ,1 ,ShapeSet, YXBanded);
}

void MainWindow::resizeTop(QMouseEvent *mouseEvent)
{
    int offsetY = mouseEvent->y() - dragStartY;
    recordY = std::max(std::min(dragRecordY + offsetY, dragRecordY + dragRecordHeight - RECORD_MIN_SIZE), 1);
    recordHeight = std::max(std::min(dragRecordHeight - offsetY, rootWindowRect.height), RECORD_MIN_SIZE);
}

void MainWindow::resizeBottom(QMouseEvent *mouseEvent)
{
    int offsetY = mouseEvent->y() - dragStartY;
    recordHeight = std::max(std::min(dragRecordHeight + offsetY, rootWindowRect.height), RECORD_MIN_SIZE);
}

void MainWindow::resizeLeft(QMouseEvent *mouseEvent)
{
    int offsetX = mouseEvent->x() - dragStartX;
    recordX = std::max(std::min(dragRecordX + offsetX, dragRecordX + dragRecordWidth - RECORD_MIN_SIZE), 1);
    recordWidth = std::max(std::min(dragRecordWidth - offsetX, rootWindowRect.width), RECORD_MIN_SIZE);
}

void MainWindow::resizeRight(QMouseEvent *mouseEvent)
{
    int offsetX = mouseEvent->x() - dragStartX;
    recordWidth = std::max(std::min(dragRecordWidth + offsetX, rootWindowRect.width), RECORD_MIN_SIZE);
}

int MainWindow::getAction(QEvent *event) {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    int cursorX = mouseEvent->x();
    int cursorY = mouseEvent->y();

    int recordButtonX = recordX + (recordWidth - RECORD_BUTTON_AREA_WIDTH) / 2;
    int recordButtonY = recordY + (recordHeight - RECORD_BUTTON_AREA_HEIGHT - RECORD_OPTIONS_AREA_HEIGHT - RECORD_OPTIONS_AREA_PADDING) / 2;
    int recordButtonWidth = RECORD_BUTTON_AREA_WIDTH;
    int recordButtonHeight = RECORD_BUTTON_AREA_HEIGHT + RECORD_OPTIONS_AREA_HEIGHT + RECORD_OPTIONS_AREA_PADDING;

    if (cursorX > recordX - CURSOR_BOUND
        && cursorX < recordX + CURSOR_BOUND
        && cursorY > recordY - CURSOR_BOUND
        && cursorY < recordY + CURSOR_BOUND) {
        // Top-Left corner.
        return ACTION_RESIZE_TOP_LEFT;
    } else if (cursorX > recordX + recordWidth - CURSOR_BOUND
               && cursorX < recordX + recordWidth + CURSOR_BOUND
               && cursorY > recordY + recordHeight - CURSOR_BOUND
               && cursorY < recordY + recordHeight + CURSOR_BOUND) {
        // Bottom-Right corner.
        return ACTION_RESIZE_BOTTOM_RIGHT;
    } else if (cursorX > recordX + recordWidth - CURSOR_BOUND
               && cursorX < recordX + recordWidth + CURSOR_BOUND
               && cursorY > recordY - CURSOR_BOUND
               && cursorY < recordY + CURSOR_BOUND) {
        // Top-Right corner.
        return ACTION_RESIZE_TOP_RIGHT;
    } else if (cursorX > recordX - CURSOR_BOUND
               && cursorX < recordX + CURSOR_BOUND
               && cursorY > recordY + recordHeight - CURSOR_BOUND
               && cursorY < recordY + recordHeight + CURSOR_BOUND) {
        // Bottom-Left corner.
        return ACTION_RESIZE_BOTTOM_LEFT;
    } else if (cursorX > recordX - CURSOR_BOUND
               && cursorX < recordX + CURSOR_BOUND) {
        // Left.
        return ACTION_RESIZE_LEFT;
    } else if (cursorX > recordX + recordWidth - CURSOR_BOUND
               && cursorX < recordX + recordWidth + CURSOR_BOUND) {
        // Right.
        return ACTION_RESIZE_RIGHT;
    } else if (cursorY > recordY - CURSOR_BOUND
               && cursorY < recordY + CURSOR_BOUND) {
        // Top.
        return ACTION_RESIZE_TOP;
    } else if (cursorY > recordY + recordHeight - CURSOR_BOUND
               && cursorY < recordY + recordHeight + CURSOR_BOUND) {
        // Bottom.
        return ACTION_RESIZE_BOTTOM;
    } else if (cursorX > recordButtonX
               && cursorX < recordButtonX + recordButtonWidth
               && cursorY > recordButtonY
               && cursorY < recordButtonY + recordButtonHeight) {
        return ACTION_STAY;
    } else {
        return ACTION_MOVE;
    }
}

void MainWindow::updateCursor(QEvent *event)
{
    if (recordButtonStatus == RECORD_BUTTON_NORMAL) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        int cursorX = mouseEvent->x();
        int cursorY = mouseEvent->y();

        int recordButtonX = recordX + (recordWidth - RECORD_BUTTON_AREA_WIDTH) / 2;
        int recordButtonY = recordY + (recordHeight - RECORD_BUTTON_AREA_HEIGHT - RECORD_OPTIONS_AREA_HEIGHT - RECORD_OPTIONS_AREA_PADDING) / 2;
        int recordButtonWidth = RECORD_BUTTON_AREA_WIDTH;
        int recordButtonHeight = RECORD_BUTTON_AREA_HEIGHT + RECORD_OPTIONS_AREA_HEIGHT + RECORD_OPTIONS_AREA_PADDING;

        if (cursorX > recordX - CURSOR_BOUND
            && cursorX < recordX + CURSOR_BOUND
            && cursorY > recordY - CURSOR_BOUND
            && cursorY < recordY + CURSOR_BOUND) {
            // Top-Left corner.
            QApplication::setOverrideCursor(Qt::SizeFDiagCursor);
        } else if (cursorX > recordX + recordWidth - CURSOR_BOUND
                   && cursorX < recordX + recordWidth + CURSOR_BOUND
                   && cursorY > recordY + recordHeight - CURSOR_BOUND
                   && cursorY < recordY + recordHeight + CURSOR_BOUND) {
            // Bottom-Right corner.
            QApplication::setOverrideCursor(Qt::SizeFDiagCursor);
        } else if (cursorX > recordX + recordWidth - CURSOR_BOUND
                   && cursorX < recordX + recordWidth + CURSOR_BOUND
                   && cursorY > recordY - CURSOR_BOUND
                   && cursorY < recordY + CURSOR_BOUND) {
            // Top-Right corner.
            QApplication::setOverrideCursor(Qt::SizeBDiagCursor);
        } else if (cursorX > recordX - CURSOR_BOUND
                   && cursorX < recordX + CURSOR_BOUND
                   && cursorY > recordY + recordHeight - CURSOR_BOUND
                   && cursorY < recordY + recordHeight + CURSOR_BOUND) {
            // Bottom-Left corner.
            QApplication::setOverrideCursor(Qt::SizeBDiagCursor);
        } else if (cursorX > recordX - CURSOR_BOUND
                   && cursorX < recordX + CURSOR_BOUND) {
            // Left.
            QApplication::setOverrideCursor(Qt::SizeHorCursor);
        } else if (cursorX > recordX + recordWidth - CURSOR_BOUND
                   && cursorX < recordX + recordWidth + CURSOR_BOUND) {
            // Right.
            QApplication::setOverrideCursor(Qt::SizeHorCursor);
        } else if (cursorY > recordY - CURSOR_BOUND
                   && cursorY < recordY + CURSOR_BOUND) {
            // Top.
            QApplication::setOverrideCursor(Qt::SizeVerCursor);
        } else if (cursorY > recordY + recordHeight - CURSOR_BOUND
                   && cursorY < recordY + recordHeight + CURSOR_BOUND) {
            // Bottom.
            QApplication::setOverrideCursor(Qt::SizeVerCursor);
        } else if (cursorX > recordButtonX
                   && cursorX < recordButtonX + recordButtonWidth
                   && cursorY > recordButtonY
                   && cursorY < recordButtonY + recordButtonHeight) {
            // Record button.
            QApplication::setOverrideCursor(Qt::ArrowCursor);
        } else {
            if (isPressButton) {
                QApplication::setOverrideCursor(Qt::ClosedHandCursor);
            } else {
                QApplication::setOverrideCursor(Qt::OpenHandCursor);
            }
        }
    }
}

void MainWindow::setDragCursor()
{
    QApplication::setOverrideCursor(Qt::CrossCursor);
}

void MainWindow::resetCursor()
{
    QApplication::overrideCursor();
}

void MainWindow::iconActivated(QSystemTrayIcon::ActivationReason)
{
    stopRecord();
}

void MainWindow::stopRecord()
{
    if (recordButtonStatus == RECORD_BUTTON_RECORDING) {
        recordProcess.stopRecord();
    }
}

void MainWindow::clearTooltip()
{
    QVector<uint32_t> data;
    data << 0 << 0 << 0 << 0 << 0 << 0;
    windowManager->setWindowBlur(this->winId(), data);
}

void MainWindow::renderTooltipRect(QPainter &painter, QList<QRectF> &rects, qreal opacity)
{
    foreach (auto rect, rects) {
        painter.setOpacity(opacity);
        QPainterPath path;
        path.addRoundedRect(rect, 8, 8);
        painter.fillPath(path, Qt::white);

        painter.setOpacity(0.04);
        QPen pen(QColor("#000000"));
        pen.setWidth(1);
        painter.setPen(pen);
        painter.drawPath(path);
    }

    QVector<uint32_t> data;
    foreach (auto rect, rects) {
        data << rect.x() << rect.y() << rect.width() << rect.height() << 8 << 8;
    }
    windowManager->setWindowBlur(this->winId(), data);

    painter.setOpacity(1.0);
}

void MainWindow::setFontSize(QPainter &painter, int textSize)
{
    QFont font = painter.font() ;
    font.setPointSize(textSize);
    painter.setFont(font);
}

QSize MainWindow::getRenderSize(QPainter &painter, QString string)
{
    QFontMetrics fm = painter.fontMetrics();

    int width = 0;
    int height = 0;
    foreach (auto line, string.split("\n")) {
        int lineWidth = fm.width(line);
        int lineHeight = fm.height();

        if (lineWidth > width) {
            width = lineWidth;
        }
        height += lineHeight;
    }

    return QSize(width, height);
}
