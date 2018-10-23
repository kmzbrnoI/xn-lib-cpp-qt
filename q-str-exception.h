#ifndef _XN_Q_STR_EXCEPTION_H_
#define _XN_Q_STR_EXCEPTION_H_

namespace Xn {

class QStrException {
private:
	QString m_err_msg;

public:
	QStrException(const QString& msg) : m_err_msg(msg) {}
	~QStrException() throw() {}
	QString str() const throw () { return this->m_err_msg; }
	operator QString() const { return this->m_err_msg; }
};

}//namespace Xn

#endif
