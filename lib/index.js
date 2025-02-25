const fastJson = require('../build/Release/fast_json');

module.exports = {
	parseAsync: (jsonString, callback = null) => {
		if (typeof jsonString !== 'string') {
			const error = new TypeError('Input must be a string');
			if (callback) {
				callback(error);
				return Promise.reject(error);
			}
			return Promise.reject(error);
		}

		return new Promise((resolve, reject) => {
			fastJson.parseAsync(jsonString, (err, result) => {
				if (err) {
					if (callback) callback(err);
					reject(err);
				} else {
					if (callback) callback(null, result);
					resolve(result);
				}
			});
		});
	}
};