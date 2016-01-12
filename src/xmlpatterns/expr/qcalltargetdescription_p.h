/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#ifndef Patternist_CallTargetDescription_H
#define Patternist_CallTargetDescription_H

template<typename Key, typename Value> class QHash;
template<typename T> class QList;

#include <QSharedData>

#include "qexpression_p.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

namespace QPatternist
{
    class CallSite;

    /**
     * @short Contains metadata for a callable component, such as a function or
     * template.
     *
     * CallTargetDescription can be used directly and is so for templates, but
     * can also be sub-classed which FunctionSignature do.
     *
     * @ingroup Patternist_expr
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class Q_AUTOTEST_EXPORT CallTargetDescription : public QSharedData
    {
    public:
        typedef QExplicitlySharedDataPointer<CallTargetDescription> Ptr;
        typedef QList<Ptr> List;

        CallTargetDescription(const QXmlName &name);

        /**
         * The function's name. For example, the name of the signature
         * <tt>fn:string() as xs:string</tt> is <tt>fn:string</tt>.
         */
        QXmlName name() const;

        /**
         * Flags callsites to be aware of their recursion by calling
         * UserFunctionCallsite::configureRecursion(), if that is the case.
         *
         * @note We pass @p expr by value here intentionally.
         */
        static void checkCallsiteCircularity(CallTargetDescription::List &signList,
                                             const Expression::Ptr expr);
    private:
        /**
         * Helper function for checkCallsiteCircularity(). If C++ allowed it,
         * it would have been local to it.
         */
        static void checkArgumentsCircularity(CallTargetDescription::List &signList,
                                              const Expression::Ptr callsite);

        Q_DISABLE_COPY(CallTargetDescription)
        const QXmlName m_name;
    };
}

QT_END_NAMESPACE

QT_END_HEADER

#endif

