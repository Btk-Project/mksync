/**
 * @file rect_algorithm.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-03-28
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <QRect>

namespace detail
{
    /**
     * @brief 矩形区域测试1
     * @param[in] rect1
     * @param[in] rect2
     * @return int
     * 0:未知关系
     * 1:rect1在rect2内部
     * 2:rect2在rect1内部
     * 3:rect1与rect2相交
     */
    inline int is_rect_inside(const QRectF &rect1, const QRectF &rect2)
    {
        if (rect1.left() >= rect2.left() && rect1.right() <= rect2.right() &&
            rect1.top() >= rect2.top() && rect1.bottom() <= rect2.bottom()) {
            return 1;
        }
        if (rect1.left() <= rect2.left() && rect1.right() >= rect2.right() &&
            rect1.top() <= rect2.top() && rect1.bottom() >= rect2.bottom()) {
            return 2;
        }
        if (rect1.left() < rect2.right() && rect1.right() > rect2.left() &&
            rect1.top() < rect2.bottom() && rect1.bottom() > rect2.top()) {
            return 3;
        }
        return 0;
    }
    /**
     * @brief 矩形区域测试-竖直/水平区域重叠
     * @param[in] rect1
     * @param[in] rect2
     * @return int
     * 0:未知关系
     * 1:rect1与rect2竖直区域重叠
     * 2:rect1与rect2水平区域重叠
     */
    inline int is_rect_overlap(const QRectF &rect1, const QRectF &rect2)
    {
        auto ret = 0;
        if (rect1.left() < rect2.right() && rect1.right() > rect2.left()) {
            ret |= 1;
        }
        if (rect1.top() < rect2.bottom() && rect1.bottom() > rect2.top()) {
            ret |= 2;
        }
        return ret;
    }
    /**
     * @brief 矩形区域测试-外部4个方向
     * @param[in] rect1
     * @param[in] rect2
     * @return int
     * 0:未知关系
     * 1:rect1在rect2上方
     * 2:rect1在rect2下方
     * 3:rect1在rect2左侧
     * 4:rect1在rect2右侧
     */
    inline int is_rect_around(const QRectF &rect1, const QRectF &rect2)
    {
        auto overlap = is_rect_overlap(rect1, rect2);
        if (overlap == 1) {
            if (rect1.bottom() >= rect2.top()) {
                return 1;
            }
            if (rect1.top() <= rect2.bottom()) {
                return 2;
            }
        }
        else if (overlap == 2) {
            if (rect1.right() <= rect2.left()) {
                return 3;
            }
            if (rect1.left() >= rect2.right()) {
                return 4;
            }
        }
        return 0;
    }
    /**
     * @brief 矩形贴边测试
     * @param[in] rect1
     * @param[in] rect2
     * @return int
     * 0:未知关系
     * 1:rect1左侧交rect2右侧
     * 2:rect1右侧交rect2左侧
     * 3:rect1上侧交rect2下侧
     * 4:rect1下侧交rect2上侧
     */
    inline int is_rect_touching(const QRectF &rect1, const QRectF &rect2)
    {
        if (rect1.left() < rect2.right() && rect1.right() > rect2.left() &&
            rect1.top() < rect2.bottom() && rect1.bottom() > rect2.top()) {
            if (rect1.left() == rect2.right()) {
                return 1;
            }
            if (rect1.right() == rect2.left()) {
                return 2;
            }
            if (rect1.top() == rect2.bottom()) {
                return 3;
            }
            if (rect1.bottom() == rect2.top()) {
                return 4;
            }
        }
        return 0;
    }

} // namespace detail