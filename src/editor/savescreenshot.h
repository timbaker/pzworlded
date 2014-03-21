#include <QThread>
#include <QPixmap>
class SaveScreenshot : public QThread
{
	Q_OBJECT

	public:
		QPixmap pixMap;
		int x;
		int y;
		void run() Q_DECL_OVERRIDE;
};

