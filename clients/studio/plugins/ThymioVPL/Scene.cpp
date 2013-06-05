#include <QGraphicsSceneMouseEvent>
#include <QMimeData>
#include <QDebug>

#include "Scene.h"
#include "Card.h"

using namespace std;

namespace Aseba { namespace ThymioVPL
{

	Scene::Scene(QObject *parent) : 
		QGraphicsScene(parent),
		prevNewEventButton(false),
		prevNewActionButton(false),
		lastFocus(-1),
		thymioCompiler(),
		eventButtonColor(QColor(0,191,255)),
		actionButtonColor(QColor(218,112,214)),
		sceneModified(false),
		newRow(false),
		advancedMode(false)
	{
		// create initial button
		EventActionPair *button = createNewButtonSet();
		buttonSetHeight = button->boundingRect().height();
		
		connect(this, SIGNAL(selectionChanged()), this, SIGNAL(stateChanged()));
	}
	
	Scene::~Scene()
	{
		clear();
	}

	QGraphicsItem *Scene::addAction(Card *item) 
	{
		EventActionPair *button = 0; 
		prevNewActionButton = false;
		
		if( newRow )
			button = createNewButtonSet();
		else if( prevNewEventButton )
			button = buttonSets.last();
		else 
		{
			if( lastFocus >= 0 )
				button = buttonSets.at(lastFocus);
			else
				for(int i=0; i<buttonSets.size(); ++i)
					if( buttonSets.at(i)->hasFocus() )
					{
						button = buttonSets.at(i);
						break;
					}
			
			if( !button )
			{
				if(!buttonSets.isEmpty() && !(buttonSets.last()->actionExists()) )
					button = buttonSets.last();
				else
				{
					button = createNewButtonSet();
					prevNewActionButton = true;
				}
			}
		}
		
		if( item ) button->addActionButton(item);
		prevNewEventButton = false;
		newRow = false;
	
		if( button->eventExists() ) 
		{
			lastFocus = -1;
			setFocusItem(0);
		}

		return button;
	}

	QGraphicsItem *Scene::addEvent(Card *item) 
	{
		EventActionPair *button = 0; 
		prevNewEventButton = false;

		if( newRow )
			button = createNewButtonSet();
		else if( prevNewActionButton )
			button = buttonSets.last();
		else 
		{
			if( lastFocus >= 0 )
				button = buttonSets.at(lastFocus);
			else
				for(int i=0; i<buttonSets.size(); ++i)
					if( buttonSets.at(i)->hasFocus() )
					{
						button = buttonSets.at(i);
						break;
					}
			
			if( !button )
			{
				if(!buttonSets.isEmpty() && !(buttonSets.last()->eventExists()) )
					button = buttonSets.last();
				else
				{
					button = createNewButtonSet();
					prevNewEventButton = true;
				}
			}

		}
		
		if( item )
			button->addEventButton(item);
		prevNewActionButton = false;
		newRow = false;
		
		if( button->actionExists() ) 
		{
			lastFocus = -1;
			setFocusItem(0);
		}
		
		return button;
	}

	void Scene::addButtonSet(Card *event, Card *action)
	{
		EventActionPair *button = createNewButtonSet();
		if(event)
			button->addEventButton(event);
		if(action)
			button->addActionButton(action);
		lastFocus = -1;
		setFocusItem(0);

		prevNewEventButton = false;
		prevNewActionButton = false;
		newRow = false;
	}

	EventActionPair *Scene::createNewButtonSet()
	{
		EventActionPair *button = new EventActionPair(buttonSets.size(), advancedMode);
		button->setColorScheme(eventButtonColor, actionButtonColor);
		buttonSets.push_back(button);
		
		addItem(button);

		connect(button, SIGNAL(buttonUpdated()), this, SLOT(buttonUpdateDetected()));
		thymioCompiler.addButtonSet(button->getIRButtonSet());
		
		return button;
	}

	bool Scene::isEmpty() const
	{
		if( buttonSets.isEmpty() )
			return true;
			
		if( buttonSets.size() > 1 ||
			buttonSets[0]->actionExists() ||
			buttonSets[0]->eventExists() )
			return false;

		return true;
	}

	void Scene::reset()
	{
		clear();
		createNewButtonSet();
		buttonUpdateDetected();
	}
	
	void Scene::clear()
	{
		for(int i=0; i<buttonSets.size(); i++)
		{
			disconnect(buttonSets.at(i), SIGNAL(buttonUpdated()), this, SLOT(buttonUpdateDetected()));
			removeItem(buttonSets.at(i));
			delete(buttonSets.at(i));
		}
		buttonSets.clear();
		thymioCompiler.clear();
		
		sceneModified = false;
		
		lastFocus = -1;
		prevNewActionButton = false;
		prevNewEventButton = false;
		advancedMode = false;
	}
	
	void Scene::haveAtLeastAnEmptyCard()
	{
		if (buttonSets.empty())
			createNewButtonSet();
	}
	
	void Scene::setColorScheme(QColor eventColor, QColor actionColor)
	{
		eventButtonColor = eventColor;
		actionButtonColor = actionColor;

		for(ButtonSetItr itr = buttonsBegin(); itr != buttonsEnd(); ++itr)
			(*itr)->setColorScheme(eventColor, actionColor);

		update();
	
		sceneModified = true;
	}

	void Scene::setAdvanced(bool advanced)
	{
		advancedMode = advanced;
		for(ButtonSetItr itr = buttonsBegin(); itr != buttonsEnd(); ++itr)
		{
			EventActionPair* button(*itr);
			button->disconnect(SIGNAL(buttonUpdated()), this, SLOT(buttonUpdateDetected()));
			button->setAdvanced(advanced);
			connect(button, SIGNAL(buttonUpdated()), SLOT(buttonUpdateDetected()));
		}
		
		buttonUpdateDetected();
	}
	
	bool Scene::isAnyStateFilter() const
	{
		for (ButtonSetConstItr itr = buttonSets.begin(); itr != buttonSets.end(); ++itr)
			if ((*itr)->isAnyStateFilter())
				return true;
		return false;
	}

	void Scene::removeButton(int row)
	{
		Q_ASSERT( row < buttonSets.size() );
		
		EventActionPair *button = buttonSets[row];
		disconnect(button, SIGNAL(buttonUpdated()), this, SLOT(buttonUpdateDetected()));
		buttonSets.removeAt(row);
		thymioCompiler.removeButtonSet(row);
		
		removeItem(button);
		delete(button);
		
		rearrangeButtons(row);

		if (buttonSets.isEmpty()) 
			createNewButtonSet();

		prevNewEventButton = false;
		prevNewActionButton = false;
		lastFocus = -1;
	
		sceneModified = true;
		
		buttonUpdateDetected();
	}
	
	void Scene::insertButton(int row) 
	{
		Q_ASSERT( row <= buttonSets.size() );

		EventActionPair *button = new EventActionPair(row, advancedMode);
		button->setColorScheme(eventButtonColor, actionButtonColor);
		buttonSets.insert(row, button);
		addItem(button);
		
		thymioCompiler.insertButtonSet(row, button->getIRButtonSet());
		
		connect(button, SIGNAL(buttonUpdated()), this, SLOT(buttonUpdateDetected()));
		
		setFocusItem(button);
		lastFocus = row;
		
		rearrangeButtons(row+1);

		prevNewEventButton = false;
		prevNewActionButton = false;
	
		sceneModified = true;
		
		buttonUpdateDetected();
	}

	void Scene::rearrangeButtons(int row)
	{
		for(int i=row; i<buttonSets.size(); ++i) 
			buttonSets.at(i)->setRow(i);
	}
	
	QString Scene::getErrorMessage() const
	{ 
		switch(thymioCompiler.getErrorCode()) {
		case THYMIO_MISSING_EVENT:
			return tr("Line %0: Missing event").arg(thymioCompiler.getErrorLine());
			break;
		case THYMIO_MISSING_ACTION:
			return tr("Line %0: Missing action").arg(thymioCompiler.getErrorLine());
			break;
		case THYMIO_EVENT_MULTISET:
			return tr("Line %0: Redeclaration of event").arg(thymioCompiler.getErrorLine());
			break;
		case THYMIO_INVALID_CODE:
			return tr("Line %0: Unknown event/action type").arg(thymioCompiler.getErrorLine());
			break;
		case THYMIO_NO_ERROR:
			return tr("Compilation success");
		default:
			return tr("Unknown VPL error");
			break;
		}
	}	
	
	QList<QString> Scene::getCode() const
	{
		QList<QString> out;
		
		for(std::vector<std::wstring>::const_iterator itr = thymioCompiler.beginCode();
			itr != thymioCompiler.endCode(); ++itr)
			out.push_back(QString::fromStdWString(*itr));
	
		return out;
	}

	int Scene::getFocusItemId() const 
	{ 
		QGraphicsItem *item = focusItem();
		
		while( item != 0 )
		{
			if( item->data(0).toString() == "buttonset" )
				return thymioCompiler.buttonToCode(item->data(1).toInt());
			else
				item = item->parentItem();
		}

		return -1;
	}
	
	void Scene::buttonUpdateDetected()
	{
		sceneModified = true;
		
		thymioCompiler.compile();
		thymioCompiler.generateCode();
		
		for(int i=0; i<buttonSets.size(); i++)
			buttonSets[i]->setErrorStatus(false);
		
		if( !thymioCompiler.isSuccessful() )
			buttonSets[thymioCompiler.getErrorLine()]->setErrorStatus(true);
		
		emit stateChanged();
	}

	void Scene::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
	{
		if( event->mimeData()->hasFormat("thymiobutton") && 
			((event->scenePos().y()-20)/(buttonSetHeight)) >= buttonSets.size() ) 
		{
			setFocusItem(0);
			event->accept();
		} 
		else
			QGraphicsScene::dragMoveEvent(event);
	}
	
	void Scene::dropEvent(QGraphicsSceneDragDropEvent *event)
	{
		if ( event->mimeData()->hasFormat("EventActionPair") )
		{
			QByteArray buttonData = event->mimeData()->data("EventActionPair");
			QDataStream dataStream(&buttonData, QIODevice::ReadOnly);

			int prevRow, currentRow;
			dataStream >> prevRow;
			
			EventActionPair *button = buttonSets.at(prevRow);
			buttonSets.removeAt(prevRow);
			thymioCompiler.removeButtonSet(prevRow);

			qreal currentYPos = event->scenePos().y();
			currentRow = (int)((currentYPos-20)/(buttonSetHeight));
			currentRow = (currentRow < 0 ? 0 : currentRow > buttonSets.size() ? buttonSets.size() : currentRow);

			buttonSets.insert(currentRow, button);
			thymioCompiler.insertButtonSet(currentRow, button->getIRButtonSet());

			rearrangeButtons( prevRow < currentRow ? prevRow : currentRow );
			
			event->setDropAction(Qt::MoveAction);
			event->accept();

			setFocusItem(button);
			lastFocus = currentRow;
			
			buttonUpdateDetected();
		}
		else if( event->mimeData()->hasFormat("thymiobutton") && 
				((event->scenePos().y()-20)/(buttonSetHeight)) >= buttonSets.size() )
		{
			QByteArray buttonData = event->mimeData()->data("thymiobutton");
			QDataStream dataStream(&buttonData, QIODevice::ReadOnly);
			
			int parentID;
			dataStream >> parentID;

			QString buttonName;
			int numButtons;
			dataStream >> buttonName >> numButtons;
			
			Card *button(Card::createButton(buttonName, advancedMode));
			if( button ) 
			{
				event->setDropAction(Qt::MoveAction);
				event->accept();
				
				lastFocus = -1;
				prevNewActionButton = false;
				prevNewEventButton = false;
				newRow = true;
				
				if( event->mimeData()->data("thymiotype") == QString("event").toLatin1() )
					addEvent(button);
				else if( event->mimeData()->data("thymiotype") == QString("action").toLatin1() )
					addAction(button);

				for( int i=0; i<numButtons; ++i ) 
				{
					int status;
					dataStream >> status;
					button->setValue(i, status);
				}
			}
		}
		else
			QGraphicsScene::dropEvent(event);
	}

	void Scene::mousePressEvent(QGraphicsSceneMouseEvent *event)
	{		
		QGraphicsScene::mousePressEvent(event);
	}
	
	void Scene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
	{
		QGraphicsScene::mouseReleaseEvent(event);
		
		if( event->button() == Qt::LeftButton ) 
		{
			QGraphicsItem *item = focusItem();
			if( item )
			{
				if( item->data(0) == "remove" ) 
					removeButton((item->parentItem()->data(1)).toInt());
				else if( item->data(0) == "add" )
					insertButton((item->parentItem()->data(1)).toInt()+1);
//				else if( item->data(0) == "buttonset" )
//					lastFocus = item->data(1).toInt();
			}
		}
		
		update();
	}
} } // namespace ThymioVPL / namespace Aseba