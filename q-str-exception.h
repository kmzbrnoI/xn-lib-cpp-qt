#ifndef XN_Q_STR_EXCEPTION_H
#define XN_Q_STR_EXCEPTION_H

#include <QString>

namespace Xn {

class QStrException {
private:
	QString m_err_msg;

public:
	QStrException(const QString& msg) : m_err_msg(msg) {}
	~QStrException() noexcept {}
	QString str() const noexcept { return this->m_err_msg; }
	operator QString() const { return this->m_err_msg; }
};

}//namespace Xn

#endif
