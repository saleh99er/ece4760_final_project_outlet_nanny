from flask import Flask, render_template

app = Flask(__name__)
app_debug  = 0 #set 1 if wish to debug with web app

@app.route('/')
@app.route('/index')
def index():
	return render_template('index.html', debug=app_debug, flask_check="Flask substituted this text")
	
if __name__ == "__main__":
	app.run(debug=True, port=5002)
