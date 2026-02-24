#include "AnimatedTabBar.h"

#include <QPainter>
#include <QResizeEvent>

AnimatedTabBar::AnimatedTabBar(QWidget *parent) : QWidget(parent) {
  setFixedHeight(48);
  m_layout = new QHBoxLayout(this);
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setSpacing(0);

  m_anim = new QPropertyAnimation(this, "highlightX", this);
  m_anim->setDuration(250);
  m_anim->setEasingCurve(QEasingCurve::InOutCubic);
}

int AnimatedTabBar::addTab(const QString &text) {
  int idx = m_tabs.size();
  auto *btn = new QPushButton(text, this);
  btn->setCheckable(true);
  btn->setAutoExclusive(true);
  btn->setCursor(Qt::PointingHandCursor);
  btn->setStyleSheet(
      "QPushButton {"
      "  border: 1px solid rgba(255, 255, 255, 0.05); background: rgba(255, "
      "255, 255, 0.02);"
      "  color: #AAAAAA; font-size: 13px;"
      "  border-radius: 6px;"
      "  margin: 0 4px;"
      "}"
      "QPushButton:checked  { background: rgba(255, 255, 255, 0.15); color: "
      "#FFFFFF; border: 1px solid rgba(255, 255, 255, 0.1); }"
      "QPushButton:hover    { background: rgba(255, 255, 255, 0.1); }");

  connect(btn, &QPushButton::clicked, this,
          [this, idx]() { setCurrentIndex(idx); });

  m_layout->addWidget(btn);
  m_tabs.push_back(btn);

  if (idx == 0)
    btn->setChecked(true);
  return idx;
}

void AnimatedTabBar::setCurrentIndex(int idx) {
  if (idx < 0 || idx >= m_tabs.size())
    return;
  m_current = idx;
  m_tabs[idx]->setChecked(true);
  updateHighlight(true);
  emit currentChanged(idx);
}

void AnimatedTabBar::setHighlightX(int x) {
  m_hlX = x;
  update(); // trigger repaint
}

void AnimatedTabBar::updateHighlight(bool animate) {
  if (m_tabs.isEmpty())
    return;
  auto *btn = m_tabs[m_current];
  int targetX = btn->x();
  m_hlWidth = btn->width();

  if (animate) {
    m_anim->stop();
    m_anim->setStartValue(m_hlX);
    m_anim->setEndValue(targetX);
    m_anim->start();
  } else {
    m_hlX = targetX;
    update();
  }
}

void AnimatedTabBar::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  // Recalculate after layout
  QMetaObject::invokeMethod(
      this, [this]() { updateHighlight(false); }, Qt::QueuedConnection);
}

void AnimatedTabBar::paintEvent(QPaintEvent * /*e*/) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // Background is transparent or subtle gray depending on integration
  p.fillRect(rect(), QColor(20, 20, 20)); // Match bottom bar

  // No top separator line

  // No animated highlight for gray static UI theme
}
